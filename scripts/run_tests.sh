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

# 每次测试使用独立 HOME，避免 ~/.myshell_history 或前序测试污染 history 等用例
run_shell() {
    local test_home
    test_home="$(mktemp -d)"
    HOME="$test_home" "$MYSHELL" < "$1"
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
        echo "  got: $(cat "$file" 2>/dev/null || echo '(missing)')"
        fail=$((fail + 1))
    fi
}

# T1: echo
SCRIPT="$TMP/t1.txt"
cat > "$SCRIPT" <<'EOF'
echo hello world
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "echo hello world" "$OUT" "hello world"

# T2: type cd / type ls
SCRIPT="$TMP/t2.txt"
cat > "$SCRIPT" <<'EOF'
type cd
type ls
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "type cd" "$OUT" "cd is a shell builtin"
assert_contains "type ls" "$OUT" "ls is"

# T3: cd
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
run_shell "$SCRIPT" >/dev/null
assert_file_eq "redirect > and >>" "$OUTFILE" $'first\nsecond'

# T5: pipe
SCRIPT="$TMP/t5.txt"
cat > "$SCRIPT" <<'EOF'
grep root /etc/passwd | wc -l
exit
EOF
OUT="$(run_shell "$SCRIPT" | tr -d ' \n')
if [[ "$OUT" =~ ^[0-9]+$ ]] && [[ "$OUT" -ge 1 ]]; then
    echo "[PASS] pipe"
    pass=$((pass + 1))
else
    echo "[FAIL] pipe"
    echo "  got: $OUT"
    fail=$((fail + 1))
fi

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
unalias hi
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "alias expand" "$OUT" "alias_ok"

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

# T10: external grep
SCRIPT="$TMP/t10.txt"
cat > "$SCRIPT" <<'EOF'
grep root /etc/passwd
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "grep external" "$OUT" "root"

# T11: quoted redirect path (path with space)
QOUT="$TMP/q out.txt"
SCRIPT="$TMP/t11.txt"
{
    printf 'echo quoted > "%s"\n' "$QOUT"
    printf 'exit\n'
} > "$SCRIPT"
run_shell "$SCRIPT" >/dev/null
assert_file_eq "quoted redirect path" "$QOUT" "quoted"

# T12: cat external
SCRIPT="$TMP/t12.txt"
cat > "$SCRIPT" <<'EOF'
cat /etc/passwd
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "cat external" "$OUT" "root"

# T13: ls external
SCRIPT="$TMP/t13.txt"
cat > "$SCRIPT" <<'EOF'
ls /bin/ls
exit
EOF
OUT="$(run_shell "$SCRIPT")"
if [[ -x /bin/ls ]]; then
    assert_contains "ls external" "$OUT" "/bin/ls"
else
    echo "[PASS] ls external (skipped: /bin/ls missing)"
    pass=$((pass + 1))
fi

# T14: background
SCRIPT="$TMP/t14.txt"
cat > "$SCRIPT" <<'EOF'
sleep 0.1 &
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "background &" "$OUT" "running in background"

# T15: background with trailing spaces / quoted & not treated as background
SCRIPT="$TMP/t15.txt"
cat > "$SCRIPT" <<'EOF'
echo "a&b"
sleep 0.1 &   
exit
EOF
OUT="$(run_shell "$SCRIPT")"
assert_contains "quoted ampersand" "$OUT" "a&b"
assert_contains "background trailing space" "$OUT" "running in background"

echo ""
echo "=============================="
echo "Results: $pass passed, $fail failed"
echo "=============================="

if [[ "$fail" -ne 0 ]]; then
    exit 1
fi
