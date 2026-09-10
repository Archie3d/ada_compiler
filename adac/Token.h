#pragma once

#include "Diagnostics.h"

#include <string>

enum class TokenKind
{
    EndOfFile,

    Identifier,
    IntegerLiteral,
    RealLiteral,
    CharacterLiteral,
    StringLiteral,

    // Reserved words (Ada 83).
    KwAbort,
    KwAbs,
    KwAccept,
    KwAccess,
    KwAll,
    KwAnd,
    KwArray,
    KwAt,
    KwBegin,
    KwBody,
    KwCase,
    KwConstant,
    KwDeclare,
    KwDelay,
    KwDelta,
    KwDigits,
    KwDo,
    KwElse,
    KwElsif,
    KwEnd,
    KwEntry,
    KwException,
    KwExit,
    KwFor,
    KwFunction,
    KwGeneric,
    KwGoto,
    KwIf,
    KwIn,
    KwIs,
    KwLimited,
    KwLoop,
    KwMod,
    KwNew,
    KwNot,
    KwNull,
    KwOf,
    KwOr,
    KwOthers,
    KwOut,
    KwPackage,
    KwPragma,
    KwPrivate,
    KwProcedure,
    KwRaise,
    KwRange,
    KwRecord,
    KwRem,
    KwRenames,
    KwReturn,
    KwReverse,
    KwSelect,
    KwSeparate,
    KwSubtype,
    KwTask,
    KwTerminate,
    KwThen,
    KwType,
    KwUse,
    KwWhen,
    KwWhile,
    KwWith,
    KwXor,

    // Single character delimiters.
    Ampersand,
    Tick,
    LeftParen,
    RightParen,
    Star,
    Plus,
    Comma,
    Minus,
    Dot,
    Slash,
    Colon,
    Semicolon,
    Less,
    Equal,
    Greater,
    Bar,

    // Compound delimiters.
    Arrow,
    DoubleDot,
    DoubleStar,
    Assign,
    NotEqual,
    GreaterEqual,
    LessEqual,
    LeftLabel,
    RightLabel,
    Box
};

struct Token
{
    TokenKind kind = TokenKind::EndOfFile;
    SourceLocation location;
    std::string text;   // Original spelling, or decoded value for literals.
    std::string lower;  // Lower-cased spelling, used for identifier lookup.
    long long intValue = 0;
    double realValue = 0.0;
};

const char* tokenKindName(TokenKind kind);
