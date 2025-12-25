/*
 * Flex Engine - Compiler Module
 *
 * Pure DSL compilation: Lexer, Parser, AST generation
 * No runtime dependencies, no rendering dependencies
 *
 * Usage:
 *   #include "flex/compiler.h"
 *   auto program = flex::parser::parse(source);
 */

#pragma once

// Compiler components
#include "flex/compiler/flex_ast.h"
#include "flex/compiler/flex_parser.h"
#include "flex/compiler/flex_token.h"

// Standard library (minimal dependencies)
#include <memory>
#include <string>
#include <vector>
