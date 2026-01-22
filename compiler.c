#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TOKEN_INT,
    TOKEN_RETURN,
    TOKEN_IDENT,
    TOKEN_NUMBER,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_SEMI,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_EOF
} TokenKind;

typedef struct {
    TokenKind kind;
    char *lexeme;
    long value;
} Token;

typedef struct {
    Token *data;
    size_t len;
    size_t cap;
} TokenList;

typedef enum {
    EXPR_NUMBER,
    EXPR_BINARY
} ExprKind;

typedef struct Expr Expr;

typedef struct {
    char op;
    Expr *left;
    Expr *right;
} BinaryExpr;

typedef struct {
    long value;
} NumberExpr;

struct Expr {
    ExprKind kind;
    union {
        NumberExpr number;
        BinaryExpr binary;
    } as;
};

typedef struct {
    Expr *value;
} ReturnStatement;

typedef struct {
    TokenList tokens;
    size_t index;
} Parser;

static void die(const char *message) {
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static void *checked_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        die("Out of memory");
    }
    return ptr;
}

static void *checked_realloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        die("Out of memory");
    }
    return new_ptr;
}

static char *copy_lexeme(const char *start, size_t len) {
    char *buffer = checked_malloc(len + 1);
    memcpy(buffer, start, len);
    buffer[len] = '\0';
    return buffer;
}

static void token_list_push(TokenList *list, Token token) {
    if (list->len == list->cap) {
        size_t new_cap = list->cap == 0 ? 16 : list->cap * 2;
        list->data = checked_realloc(list->data, new_cap * sizeof(Token));
        list->cap = new_cap;
    }
    list->data[list->len++] = token;
}

static Token make_token(TokenKind kind, const char *start, size_t len) {
    Token token;
    token.kind = kind;
    token.lexeme = copy_lexeme(start, len);
    token.value = 0;
    return token;
}

static Token make_number_token(const char *start, size_t len, long value) {
    Token token = make_token(TOKEN_NUMBER, start, len);
    token.value = value;
    return token;
}

static TokenList tokenize(const char *source) {
    TokenList list = {0};
    const char *p = source;
    while (*p) {
        if (isspace((unsigned char)*p)) {
            p++;
            continue;
        }
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            while (isalnum((unsigned char)*p) || *p == '_') {
                p++;
            }
            size_t len = (size_t)(p - start);
            if (len == 3 && strncmp(start, "int", len) == 0) {
                token_list_push(&list, make_token(TOKEN_INT, start, len));
            } else if (len == 6 && strncmp(start, "return", len) == 0) {
                token_list_push(&list, make_token(TOKEN_RETURN, start, len));
            } else {
                token_list_push(&list, make_token(TOKEN_IDENT, start, len));
            }
            continue;
        }
        if (isdigit((unsigned char)*p)) {
            const char *start = p;
            errno = 0;
            char *end = NULL;
            long value = strtol(p, &end, 10);
            if (errno != 0) {
                die("Failed to parse integer literal");
            }
            size_t len = (size_t)(end - start);
            token_list_push(&list, make_number_token(start, len, value));
            p = end;
            continue;
        }
        switch (*p) {
            case '(':
                token_list_push(&list, make_token(TOKEN_LPAREN, p, 1));
                p++;
                break;
            case ')':
                token_list_push(&list, make_token(TOKEN_RPAREN, p, 1));
                p++;
                break;
            case '{':
                token_list_push(&list, make_token(TOKEN_LBRACE, p, 1));
                p++;
                break;
            case '}':
                token_list_push(&list, make_token(TOKEN_RBRACE, p, 1));
                p++;
                break;
            case ';':
                token_list_push(&list, make_token(TOKEN_SEMI, p, 1));
                p++;
                break;
            case '+':
                token_list_push(&list, make_token(TOKEN_PLUS, p, 1));
                p++;
                break;
            case '-':
                token_list_push(&list, make_token(TOKEN_MINUS, p, 1));
                p++;
                break;
            case '*':
                token_list_push(&list, make_token(TOKEN_STAR, p, 1));
                p++;
                break;
            case '/':
                token_list_push(&list, make_token(TOKEN_SLASH, p, 1));
                p++;
                break;
            default:
                die("Unexpected character in input");
        }
    }
    token_list_push(&list, make_token(TOKEN_EOF, "", 0));
    return list;
}

static Token *parser_current(Parser *parser) {
    return &parser->tokens.data[parser->index];
}

static Token *parser_consume(Parser *parser, TokenKind kind) {
    Token *token = parser_current(parser);
    if (token->kind != kind) {
        die("Unexpected token");
    }
    parser->index++;
    return token;
}

static int parser_match(Parser *parser, TokenKind kind) {
    if (parser_current(parser)->kind == kind) {
        parser->index++;
        return 1;
    }
    return 0;
}

static Expr *parse_expr(Parser *parser);

static Expr *new_number_expr(long value) {
    Expr *expr = checked_malloc(sizeof(Expr));
    expr->kind = EXPR_NUMBER;
    expr->as.number.value = value;
    return expr;
}

static Expr *new_binary_expr(char op, Expr *left, Expr *right) {
    Expr *expr = checked_malloc(sizeof(Expr));
    expr->kind = EXPR_BINARY;
    expr->as.binary.op = op;
    expr->as.binary.left = left;
    expr->as.binary.right = right;
    return expr;
}

static Expr *parse_primary(Parser *parser) {
    if (parser_match(parser, TOKEN_LPAREN)) {
        Expr *expr = parse_expr(parser);
        parser_consume(parser, TOKEN_RPAREN);
        return expr;
    }
    Token *number = parser_consume(parser, TOKEN_NUMBER);
    return new_number_expr(number->value);
}

static Expr *parse_mul(Parser *parser) {
    Expr *expr = parse_primary(parser);
    for (;;) {
        if (parser_match(parser, TOKEN_STAR)) {
            expr = new_binary_expr('*', expr, parse_primary(parser));
        } else if (parser_match(parser, TOKEN_SLASH)) {
            expr = new_binary_expr('/', expr, parse_primary(parser));
        } else {
            break;
        }
    }
    return expr;
}

static Expr *parse_add(Parser *parser) {
    Expr *expr = parse_mul(parser);
    for (;;) {
        if (parser_match(parser, TOKEN_PLUS)) {
            expr = new_binary_expr('+', expr, parse_mul(parser));
        } else if (parser_match(parser, TOKEN_MINUS)) {
            expr = new_binary_expr('-', expr, parse_mul(parser));
        } else {
            break;
        }
    }
    return expr;
}

static Expr *parse_expr(Parser *parser) {
    return parse_add(parser);
}

static ReturnStatement parse_program(Parser *parser) {
    parser_consume(parser, TOKEN_INT);
    Token *ident = parser_consume(parser, TOKEN_IDENT);
    if (strcmp(ident->lexeme, "main") != 0) {
        die("Only main is supported");
    }
    parser_consume(parser, TOKEN_LPAREN);
    parser_consume(parser, TOKEN_RPAREN);
    parser_consume(parser, TOKEN_LBRACE);
    parser_consume(parser, TOKEN_RETURN);
    Expr *expr = parse_expr(parser);
    parser_consume(parser, TOKEN_SEMI);
    parser_consume(parser, TOKEN_RBRACE);
    parser_consume(parser, TOKEN_EOF);

    ReturnStatement stmt;
    stmt.value = expr;
    return stmt;
}

static void emit_expr(FILE *out, Expr *expr) {
    if (expr->kind == EXPR_NUMBER) {
        fprintf(out, "  mov rax, %ld\n", expr->as.number.value);
        return;
    }
    if (expr->kind == EXPR_BINARY) {
        emit_expr(out, expr->as.binary.left);
        fprintf(out, "  push rax\n");
        emit_expr(out, expr->as.binary.right);
        fprintf(out, "  pop rbx\n");
        switch (expr->as.binary.op) {
            case '+':
                fprintf(out, "  add rax, rbx\n");
                break;
            case '-':
                fprintf(out, "  sub rbx, rax\n");
                fprintf(out, "  mov rax, rbx\n");
                break;
            case '*':
                fprintf(out, "  imul rax, rbx\n");
                break;
            case '/':
                fprintf(out, "  mov rcx, rax\n");
                fprintf(out, "  mov rax, rbx\n");
                fprintf(out, "  cqo\n");
                fprintf(out, "  idiv rcx\n");
                break;
            default:
                die("Unsupported binary operator");
        }
        return;
    }
    die("Unsupported expression");
}

static void free_expr(Expr *expr) {
    if (!expr) {
        return;
    }
    if (expr->kind == EXPR_BINARY) {
        free_expr(expr->as.binary.left);
        free_expr(expr->as.binary.right);
    }
    free(expr);
}

static void free_tokens(TokenList *list) {
    for (size_t i = 0; i < list->len; i++) {
        free(list->data[i].lexeme);
    }
    free(list->data);
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror("Failed to open input file");
        exit(EXIT_FAILURE);
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        perror("Failed to seek input file");
        exit(EXIT_FAILURE);
    }
    long size = ftell(file);
    if (size < 0) {
        perror("Failed to read input file size");
        exit(EXIT_FAILURE);
    }
    rewind(file);
    char *buffer = checked_malloc((size_t)size + 1);
    size_t read = fread(buffer, 1, (size_t)size, file);
    if (read != (size_t)size) {
        perror("Failed to read input file");
        exit(EXIT_FAILURE);
    }
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s <input.c> [-o output.s]\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *input_path = argv[1];
    const char *output_path = NULL;
    if (argc == 4) {
        if (strcmp(argv[2], "-o") != 0) {
            die("Expected -o for output path");
        }
        output_path = argv[3];
    }

    char *source = read_file(input_path);
    TokenList tokens = tokenize(source);
    Parser parser = {tokens, 0};
    ReturnStatement program = parse_program(&parser);

    FILE *out = stdout;
    if (output_path) {
        out = fopen(output_path, "w");
        if (!out) {
            perror("Failed to open output file");
            return EXIT_FAILURE;
        }
    }

    fprintf(out, ".intel_syntax noprefix\n");
    fprintf(out, ".globl main\n");
    fprintf(out, "main:\n");
    emit_expr(out, program.value);
    fprintf(out, "  ret\n");

    if (output_path) {
        fclose(out);
    }

    free_expr(program.value);
    free_tokens(&tokens);
    free(source);

    return EXIT_SUCCESS;
}
