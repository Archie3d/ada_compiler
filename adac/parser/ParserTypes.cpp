#include "Parser.h"

#include <utility>

DeclPtr Parser::parseTypeDecl()
{
    auto decl = std::make_unique<TypeDecl>();
    decl->location = current().location;
    expect(TokenKind::KwType, "in type declaration");
    const Token& name = expect(TokenKind::Identifier, "in type declaration");
    decl->name = name.text;
    decl->lower = name.lower;

    // 'type Shape (Kind : Figure)' names the discriminants an object of the
    // type is fixed with when it is declared.
    if (check(TokenKind::LeftParen)) {
        parseDiscriminantPart(decl->discriminants);
    }

    // 'type Node;' names a type and says no more about it, which is how a
    // record and an access type pointing at it are declared in either order.
    if (match(TokenKind::Semicolon)) {
        return decl;
    }

    expect(TokenKind::KwIs, "in type declaration");
    decl->definition = parseTypeDefinition();
    expect(TokenKind::Semicolon, "after type declaration");
    return decl;
}

DeclPtr Parser::parseSubtypeDecl()
{
    auto decl = std::make_unique<SubtypeDecl>();
    decl->location = current().location;
    expect(TokenKind::KwSubtype, "in subtype declaration");
    const Token& name = expect(TokenKind::Identifier, "in subtype declaration");
    decl->name = name.text;
    decl->lower = name.lower;
    expect(TokenKind::KwIs, "in subtype declaration");
    decl->subtype = parseSubtypeIndication();
    expect(TokenKind::Semicolon, "after subtype declaration");
    return decl;
}

TypeDefinitionPtr Parser::parseTypeDefinition()
{
    SourceLocation location = current().location;

    if (check(TokenKind::LeftParen)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::Enumeration);
        definition->location = location;
        advance();
        while (true) {
            if (check(TokenKind::Identifier) || check(TokenKind::CharacterLiteral)) {
                const Token& literal = advance();
                definition->literals.push_back(literal.text);
                definition->literalsLower.push_back(literal.lower);
            } else {
                fail("expected an enumeration literal");
            }
            if (!match(TokenKind::Comma)) {
                break;
            }
        }
        expect(TokenKind::RightParen, "after enumeration literals");
        return definition;
    }

    // 'is private' and 'is limited private' name a type without saying what it
    // is made of; the private part of the package says that.
    if (check(TokenKind::KwPrivate) || (check(TokenKind::KwLimited) && peek(1).kind == TokenKind::KwPrivate)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::Private);
        definition->location = location;
        definition->isLimited = match(TokenKind::KwLimited);
        expect(TokenKind::KwPrivate, "in private type declaration");
        return definition;
    }

    if (check(TokenKind::KwRange)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::IntegerRange);
        definition->location = location;
        advance();
        definition->rangeLow = parseSimpleExpression();
        expect(TokenKind::DoubleDot, "in range");
        definition->rangeHigh = parseSimpleExpression();
        return definition;
    }

    if (check(TokenKind::KwDigits)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::FloatDigits);
        definition->location = location;
        advance();
        definition->digits = parseSimpleExpression();
        if (match(TokenKind::KwRange)) {
            definition->rangeLow = parseSimpleExpression();
            expect(TokenKind::DoubleDot, "in range");
            definition->rangeHigh = parseSimpleExpression();
        }
        return definition;
    }

    if (check(TokenKind::KwArray)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::Array);
        definition->location = location;
        advance();
        expect(TokenKind::LeftParen, "in array type definition");
        while (true) {
            auto index = std::make_unique<SubtypeIndication>();
            index->location = current().location;
            std::size_t saved = m_position;
            if (check(TokenKind::Identifier)) {
                index->name = parseSubtypeMark(index->lower);
                if (match(TokenKind::KwRange)) {
                    if (match(TokenKind::Box)) {
                        definition->unconstrainedIndexes = true;
                    } else {
                        index->rangeLow = parseSimpleExpression();
                        expect(TokenKind::DoubleDot, "in index range");
                        index->rangeHigh = parseSimpleExpression();
                    }
                } else if (check(TokenKind::DoubleDot)) {
                    m_position = saved;
                    index->name.clear();
                    index->lower.clear();
                    index->rangeLow = parseSimpleExpression();
                    expect(TokenKind::DoubleDot, "in index range");
                    index->rangeHigh = parseSimpleExpression();
                }
            } else {
                index->rangeLow = parseSimpleExpression();
                expect(TokenKind::DoubleDot, "in index range");
                index->rangeHigh = parseSimpleExpression();
            }
            definition->indexTypes.push_back(std::move(index));
            if (!match(TokenKind::Comma)) {
                break;
            }
        }
        expect(TokenKind::RightParen, "after array index list");
        expect(TokenKind::KwOf, "in array type definition");
        definition->elementType = parseSubtypeIndication();
        return definition;
    }

    if (check(TokenKind::KwRecord) || (check(TokenKind::KwNull) && peek(1).kind == TokenKind::KwRecord)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::Record);
        definition->location = location;
        if (match(TokenKind::KwNull)) {
            expect(TokenKind::KwRecord, "in null record definition");
            return definition;
        }
        advance();
        parseRecordComponents(definition->fields);
        // A variant part comes last, after the components every value has.
        if (check(TokenKind::KwCase)) {
            definition->variant = parseVariantPart();
        }
        expect(TokenKind::KwEnd, "at end of record definition");
        expect(TokenKind::KwRecord, "at end of record definition");
        return definition;
    }

    if (check(TokenKind::KwNew)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::Derived);
        definition->location = location;
        advance();
        definition->parent = parseSubtypeIndication();
        return definition;
    }

    if (check(TokenKind::KwAccess)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::Access);
        definition->location = location;
        advance();
        definition->parent = parseSubtypeIndication();
        return definition;
    }

    fail("unsupported type definition");
}

// '(Kind : Figure; Size : Positive)' after a type name.  The components it
// names come first in every value of the type and are fixed once an object of
// it is declared.
void Parser::parseDiscriminantPart(std::vector<RecordField>& discriminants)
{
    expect(TokenKind::LeftParen, "in discriminant part");
    while (true) {
        SourceLocation location = current().location;
        std::vector<std::string> lowered;
        std::vector<std::string> names = parseIdentifierList(lowered);
        expect(TokenKind::Colon, "in discriminant");
        SubtypeIndicationPtr subtype = parseSubtypeIndication();
        ExprPtr defaultValue;
        if (match(TokenKind::Assign)) {
            defaultValue = parseExpression();
        }

        for (std::size_t i = 0; i < names.size(); ++i) {
            RecordField field;
            field.name = names[i];
            field.lower = lowered[i];
            field.location = location;
            auto copy = std::make_unique<SubtypeIndication>();
            copy->location = subtype->location;
            copy->name = subtype->name;
            copy->lower = subtype->lower;
            field.subtype = std::move(copy);
            if (i + 1 == names.size()) {
                field.subtype = std::move(subtype);
                field.defaultValue = std::move(defaultValue);
            }
            discriminants.push_back(std::move(field));
        }

        if (!match(TokenKind::Semicolon)) {
            break;
        }
    }
    expect(TokenKind::RightParen, "after discriminant part");
}

// The component declarations of a record definition or of one alternative of a
// variant part, up to whatever ends them.
void Parser::parseRecordComponents(std::vector<RecordField>& fields)
{
    while (!check(TokenKind::KwEnd) && !check(TokenKind::KwWhen) && !check(TokenKind::KwCase)
           && !check(TokenKind::EndOfFile)) {
        if (match(TokenKind::KwNull)) {
            expect(TokenKind::Semicolon, "after null component");
            continue;
        }
        SourceLocation fieldLocation = current().location;
        std::vector<std::string> lowered;
        std::vector<std::string> names = parseIdentifierList(lowered);
        expect(TokenKind::Colon, "in record component");
        SubtypeIndicationPtr subtype = parseSubtypeIndication();
        ExprPtr defaultValue;
        if (match(TokenKind::Assign)) {
            defaultValue = parseExpression();
        }
        expect(TokenKind::Semicolon, "after record component");
        for (std::size_t i = 0; i < names.size(); ++i) {
            RecordField field;
            field.name = names[i];
            field.lower = lowered[i];
            field.location = fieldLocation;
            auto copy = std::make_unique<SubtypeIndication>();
            copy->location = subtype->location;
            copy->name = subtype->name;
            copy->lower = subtype->lower;
            field.subtype = std::move(copy);
            if (i + 1 == names.size()) {
                field.subtype = std::move(subtype);
                field.defaultValue = std::move(defaultValue);
            }
            fields.push_back(std::move(field));
        }
    }
}

// The components a record has only when its discriminant holds one of the
// values named, written as a case at the end of the record definition.
VariantPartPtr Parser::parseVariantPart()
{
    auto part = std::make_unique<VariantPart>();
    part->location = current().location;
    expect(TokenKind::KwCase, "in variant part");
    const Token& name = expect(TokenKind::Identifier, "in variant part");
    part->discriminant = name.text;
    part->discriminantLower = name.lower;
    expect(TokenKind::KwIs, "in variant part");

    while (match(TokenKind::KwWhen)) {
        RecordVariant variant;
        variant.location = current().location;
        if (match(TokenKind::KwOthers)) {
            variant.isOthers = true;
        } else {
            while (true) {
                variant.choiceLows.push_back(parseSimpleExpression());
                if (match(TokenKind::DoubleDot)) {
                    variant.choiceHighs.push_back(parseSimpleExpression());
                } else {
                    variant.choiceHighs.push_back(nullptr);
                }
                if (!match(TokenKind::Bar)) {
                    break;
                }
            }
        }
        expect(TokenKind::Arrow, "in variant alternative");
        parseRecordComponents(variant.fields);
        part->variants.push_back(std::move(variant));
    }

    expect(TokenKind::KwEnd, "at end of variant part");
    expect(TokenKind::KwCase, "at end of variant part");
    expect(TokenKind::Semicolon, "after variant part");
    return part;
}

SubtypeIndicationPtr Parser::parseSubtypeIndication()
{
    auto indication = std::make_unique<SubtypeIndication>();
    indication->location = current().location;
    indication->name = parseSubtypeMark(indication->lower);

    if (match(TokenKind::KwDigits)) {
        indication->digits = parseSimpleExpression();
    }

    if (match(TokenKind::KwRange)) {
        indication->rangeLow = parseSimpleExpression();
        expect(TokenKind::DoubleDot, "in range constraint");
        indication->rangeHigh = parseSimpleExpression();
        return indication;
    }

    if (check(TokenKind::LeftParen)) {
        advance();
        while (true) {
            ExprPtr low = parseSimpleExpression();
            ExprPtr high;
            if (match(TokenKind::DoubleDot)) {
                high = parseSimpleExpression();
            }
            indication->indexLows.push_back(std::move(low));
            indication->indexHighs.push_back(std::move(high));
            if (!match(TokenKind::Comma)) {
                break;
            }
        }
        expect(TokenKind::RightParen, "after index constraint");
    }

    return indication;
}

void Parser::parseDiscreteRange(std::string& typeName, std::string& typeLower, ExprPtr& low, ExprPtr& high)
{
    std::size_t saved = m_position;

    if (check(TokenKind::Identifier)) {
        std::string lowered;
        std::string name = parseSubtypeMark(lowered);
        if (match(TokenKind::KwRange)) {
            typeName = name;
            typeLower = lowered;
            low = parseSimpleExpression();
            expect(TokenKind::DoubleDot, "in discrete range");
            high = parseSimpleExpression();
            return;
        }
        // A name on its own is a type mark, but only where the range ends: a
        // name that something follows, as in 'Length + 1 .. Width', starts an
        // expression like any other.
        if (check(TokenKind::KwLoop) || check(TokenKind::Semicolon) || check(TokenKind::RightParen)
            || check(TokenKind::Comma) || check(TokenKind::Arrow) || check(TokenKind::Bar)) {
            typeName = name;
            typeLower = lowered;
            return;
        }
        m_position = saved;
    }

    std::size_t rangeStart = m_position;
    ExprPtr first = parseSimpleExpression();
    if (first->kind == ExprKind::Attribute) {
        auto* attribute = static_cast<AttributeExpr*>(first.get());
        if (attribute->lower == "range") {
            auto makeAttribute = [&](const char* name) {
                m_position = rangeStart;
                ExprPtr copy = parseSimpleExpression();
                auto* expr = static_cast<AttributeExpr*>(copy.get());
                expr->name = name;
                expr->lower = name;
                return copy;
            };
            low = makeAttribute("first");
            high = makeAttribute("last");
            return;
        }
    }

    low = std::move(first);
    expect(TokenKind::DoubleDot, "in discrete range");
    high = parseSimpleExpression();
}
