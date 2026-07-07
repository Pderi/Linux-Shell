#include "alias.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static AliasNode *head = NULL;

void alias_init(void)
{
    head = NULL;
}

int alias_set(const char *name, const char *value)
{
    if (!name || !*name || !value)
        return -1;

    for (AliasNode *n = head; n; n = n->next) {
        if (strcmp(n->name, name) == 0) {
            free(n->value);
            n->value = strdup(value);
            return n->value ? 0 : -1;
        }
    }

    AliasNode *node = calloc(1, sizeof(*node));
    if (!node)
        return -1;
    node->name = strdup(name);
    node->value = strdup(value);
    if (!node->name || !node->value) {
        free(node->name);
        free(node->value);
        free(node);
        return -1;
    }
    node->next = head;
    head = node;
    return 0;
}

int alias_unset(const char *name)
{
    AliasNode **pp = &head;
    while (*pp) {
        if (strcmp((*pp)->name, name) == 0) {
            AliasNode *del = *pp;
            *pp = del->next;
            free(del->name);
            free(del->value);
            free(del);
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;
}

void alias_list(void)
{
    for (AliasNode *n = head; n; n = n->next)
        printf("alias %s='%s'\n", n->name, n->value);
}

static const AliasNode *alias_find(const char *name)
{
    for (AliasNode *n = head; n; n = n->next) {
        if (strcmp(n->name, name) == 0)
            return n;
    }
    return NULL;
}

char *alias_expand_line(const char *line)
{
    if (!line || !*line)
        return NULL;

    const char *p = line;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (!*p)
        return strdup(line);

    char name[256];
    size_t i = 0;
    while (p[i] && !isspace((unsigned char)p[i]) && i + 1 < sizeof(name)) {
        name[i] = p[i];
        i++;
    }
    name[i] = '\0';

    if (strcmp(name, "alias") == 0 || strcmp(name, "unalias") == 0)
        return strdup(line);

    const AliasNode *al = alias_find(name);
    if (!al)
        return strdup(line);

    const char *rest = p + i;
    size_t new_len = strlen(al->value) + strlen(rest) + 1;
    char *out = malloc(new_len);
    if (!out)
        return NULL;
    snprintf(out, new_len, "%s%s", al->value, rest);
    return out;
}

void alias_free_all(void)
{
    while (head) {
        AliasNode *n = head;
        head = head->next;
        free(n->name);
        free(n->value);
        free(n);
    }
}

static int parse_alias_assignment(const char *arg, char *name, size_t nlen, char *value, size_t vlen)
{
    const char *eq = strchr(arg, '=');
    if (!eq || eq == arg)
        return -1;

    size_t nl = (size_t)(eq - arg);
    if (nl + 1 > nlen)
        return -1;
    memcpy(name, arg, nl);
    name[nl] = '\0';

    const char *v = eq + 1;
    if (*v == '\'' || *v == '"') {
        char q = *v++;
        const char *end = strchr(v, q);
        if (!end)
            return -1;
        size_t vl = (size_t)(end - v);
        if (vl + 1 > vlen)
            return -1;
        memcpy(value, v, vl);
        value[vl] = '\0';
        return 0;
    }

    if (strlen(v) + 1 > vlen)
        return -1;
    strcpy(value, v);
    return 0;
}

int builtin_alias(Command *cmd)
{
    if (cmd->argc == 1) {
        alias_list();
        return 0;
    }

    for (int i = 1; i < cmd->argc; i++) {
        char name[256], value[1024];
        if (parse_alias_assignment(cmd->argv[i], name, sizeof(name), value, sizeof(value)) < 0) {
            fprintf(stderr, "alias: invalid format: %s\n", cmd->argv[i]);
            return 1;
        }
        if (alias_set(name, value) < 0) {
            fprintf(stderr, "alias: out of memory\n");
            return 1;
        }
    }
    return 0;
}

int builtin_unalias(Command *cmd)
{
    if (cmd->argc < 2) {
        fprintf(stderr, "unalias: missing argument\n");
        return 1;
    }
    if (alias_unset(cmd->argv[1]) < 0) {
        fprintf(stderr, "unalias: %s: not found\n", cmd->argv[1]);
        return 1;
    }
    return 0;
}
