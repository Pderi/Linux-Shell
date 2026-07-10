#!/usr/bin/env bash
# myshell 自动化功能测试（在 Linux / WSL 下运行）
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

make -s clean
make -s

MYSHELL="$ROOT/myshell"
if [[ ! -x "$MYSHELL" ]]; then
    echo "error: $MYSHELL not found or not executable (run make first)"
    exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0

run_shell() {
    local test_home
    test_home="$(mktemp -d)"
    HOME="$test_home" "$MYSHELL" < "$1" 2>&1
    rm -rf "$test_home"
}

assert_contains() {
    local desc="$1"
    local haystack="$2"
    local needle="$3"
    if echo "$haystack" | grep -Fq "$needle"; then
        echo "[PASS] $desc"
        pass=$((pass + 1))
    else
        echo "[FAIL] $desc"
        echo "  expected to contain: $needle"
        echo "  got:"
        echo "$haystack" | sed 's/^/    /'
        fail=$((fail + 1))
    fi
}

assert_not_contains() {
    local desc="$1"
    local haystack="$2"
    local needle="$3"
    if echo "$haystack" | grep -Fq "$needle"; then
        echo "[FAIL] $desc"
        echo "  unexpected: $needle"
        fail=$((fail + 1))
    else
        echo "[PASS] $desc"
        pass=$((pass + 1))
    fi
}

assert_file_eq() {
    local desc="$1"
    local file="$2"
    local expected="$3"
    if [[ -f "$file" ]] && [[ "$(cat "$file")" == "$expected" ]]; then
        echo "[PASS] $desc"
        pass=$((pass + 1))
    else
        echo "[FAIL] $desc"
        echo "  expected file: $expected"
        echo "  got: $(cat "$file" 2>/dev/null || echo 'missing')"
        fail=$((fail + 1))
    fi
}

# T1: echo
SCRIPT="$TMP/t1.txt"
cat > "$SCRIPT" <<'EOF'
echo hello world
echo "hello quoted world"
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "echo hello world" "$OUT" "hello world"
assert_contains "echo quoted arg" "$OUT" "hello quoted world"
assert_not_contains "echo no waitpid error" "$OUT" "waitpid: No child processes"

# T2: type cd / type ls
SCRIPT="$TMP/t2.txt"
cat > "$SCRIPT" <<'EOF'
type cd
type ls
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "type cd" "$OUT" "cd is a shell builtin"
assert_contains "type ls" "$OUT" "ls is a shell builtin"

# T3: cd + pwd
SCRIPT="$TMP/t3.txt"
cat > "$SCRIPT" <<'EOF'
cd /tmp
pwd
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "cd /tmp" "$OUT" "/tmp"

# T4: redirect
OUTFILE="$TMP/out.txt"
SCRIPT="$TMP/t4.txt"
cat > "$SCRIPT" <<EOF
echo first > "$OUTFILE"
echo second >> "$OUTFILE"
cat < "$OUTFILE"
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_not_contains "redirect no waitpid error" "$OUT" "waitpid: No child processes"
assert_file_eq "redirect > and >>" "$OUTFILE" $'first\nsecond'

# T5: pipe (builtins only)
SCRIPT="$TMP/t5.txt"
cat > "$SCRIPT" <<'EOF'
grep root /etc/passwd | grep root
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "pipe builtins" "$OUT" "root"

# T6: builtin echo in pipe
SCRIPT="$TMP/t6.txt"
cat > "$SCRIPT" <<'EOF'
echo hello | grep hello
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "echo in pipe" "$OUT" "hello"

# T7: alias
SCRIPT="$TMP/t7.txt"
cat > "$SCRIPT" <<'EOF'
alias hi='echo alias_ok'
hi
alias dir='echo spaced_ok'
dir
alias ll="echo dq_ok"
ll
alias lr='ls -l'
lr
alias
unalias hi
unalias dir
unalias ll
unalias lr
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "alias expand" "$OUT" "alias_ok"
assert_contains "alias quoted space value" "$OUT" "spaced_ok"
assert_contains "alias double quoted value" "$OUT" "dq_ok"
assert_contains "alias list all" "$OUT" "alias hi='echo alias_ok'"
if echo "$OUT" | grep -Fq "invalid format"; then
    echo "[FAIL] alias ls -l style"
    fail=$((fail + 1))
else
    echo "[PASS] alias ls -l style"
    pass=$((pass + 1))
fi

# T8: history
SCRIPT="$TMP/t8.txt"
cat > "$SCRIPT" <<'EOF'
echo one
echo two
history 3
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "history echo one" "$OUT" "echo one"
assert_contains "history echo two" "$OUT" "echo two"

# T9: echo env
SCRIPT="$TMP/t9.txt"
cat > "$SCRIPT" <<'EOF'
echo $HOME
exit
EOF
OUT="$(run_shell "$SCRIPT")"
if echo "$OUT" | grep -q "/"; then
    echo "[PASS] echo \$HOME"
    pass=$((pass + 1))
else
    echo "[FAIL] echo \$HOME"
    fail=$((fail + 1))
fi

# T10: builtin grep
SCRIPT="$TMP/t10.txt"
cat > "$SCRIPT" <<'EOF'
grep root /etc/passwd
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "grep builtin" "$OUT" "root"

# T11: quoted redirect path (path with space)
QOUT="$TMP/q out.txt"
SCRIPT="$TMP/t11.txt"
{
    printf 'echo quoted > "%s"\n' "$QOUT"
    printf 'exit\n'
} > "$SCRIPT"
run_shell "$SCRIPT" >/dev/null
assert_file_eq "quoted redirect path" "$QOUT" "quoted"

# T12: builtin cat
SCRIPT="$TMP/t12.txt"
cat > "$SCRIPT" <<'EOF'
cat /etc/passwd
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "cat builtin" "$OUT" "root"

# T13: builtin ls
SCRIPT="$TMP/t13.txt"
cat > "$SCRIPT" <<'EOF'
ls /bin/ls
exit
EOF
OUT="$(run_shell "$SCRIPT")"
if [[ -e /bin/ls ]]; then
    assert_contains "ls builtin" "$OUT" "/bin/ls"
else
    echo "[PASS] ls builtin - skipped, /bin/ls missing"
    pass=$((pass + 1))
fi

# T14: background
SCRIPT="$TMP/t14.txt"
cat > "$SCRIPT" <<'EOF'
grep root /etc/passwd &
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "background &" "$OUT" "running in background"

# T15: background with trailing spaces / quoted & not treated as background
SCRIPT="$TMP/t15.txt"
cat > "$SCRIPT" <<'EOF'
echo "a&b"
grep root /etc/passwd &
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "quoted ampersand" "$OUT" "a&b"
assert_contains "background trailing space" "$OUT" "running in background"

# T16: unknown command rejected (no exec)
SCRIPT="$TMP/t16.txt"
cat > "$SCRIPT" <<'EOF'
wc -l /etc/passwd
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "reject external command" "$OUT" "wc: command not found"

# T17: sleep builtin
SCRIPT="$TMP/t17.txt"
cat > "$SCRIPT" <<'EOF'
sleep 0
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_not_contains "sleep no error" "$OUT" "error"
assert_not_contains "sleep no command not found" "$OUT" "command not found"

# T18: type sleep (verify sleep is builtin)
SCRIPT="$TMP/t18.txt"
cat > "$SCRIPT" <<'EOF'
type sleep
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "type sleep builtin" "$OUT" "sleep is a shell builtin"

# T19: unalias removes alias
SCRIPT="$TMP/t19.txt"
cat > "$SCRIPT" <<'EOF'
alias temp='echo temp_ok'
temp
unalias temp
temp
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "alias temp works" "$OUT" "temp_ok"
assert_contains "unalias removes alias" "$OUT" "temp: command not found"

echo ""
echo "=============================="
echo "Results: $pass passed, $fail failed"
echo "=============================="

if [[ "$fail" -ne 0 ]]; then
    exit 1
fi
