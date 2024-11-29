#include "Control.hpp"
#pragma once

void GenerateSizeFunction(Control& control)
{
    int indent = control.indent, original = control.indent;

    emplex::Token size_function{emplex::Lexer::ID_SIZE, "size", 0,0};
    control.symbols.AddFunction(size_function, {Type("string")}, Type("int"));

    control.CommentLine("Function to get the size of a string")
        .Code("(func $size (param $str i32) (result i32)")
        .Indent(2)
        .CommentLine("Variables")
        .Code("(local $len i32)")
        .Code("(local $i i32)")
        .CommentLine("Begin code")
        .Code("(local.set $len (i32.const 0))")
        .Comment("Set len to 0")
        .Code("(local.set $i (local.get $str))")
        .Comment("Set i to the starting index of str")
        .CommentLine("")
        .CommentLine("Begin Loop")
        .Indent(2)
        .Code("(block $exit_while")
        .Indent(2)
        .Code("(loop $while")
        .CommentLine("While test condition")
        .Code("(i32.load8_u (local.get $i))")
        .Comment("Stack.push str[i]")
        .Code("(i32.eqz)")
        .Comment("Check if we loaded a nullterm")
        .Code("(br_if $exit_while)")
        .Comment("break")
        .CommentLine("")
        .CommentLine("While body")
        .Code("(i32.add (local.get $len) (i32.const 1))")
        .Comment("len + 1")
        .Code("(local.set $len)").Comment("len = len + 1")
        .Code("(i32.add (local.get $i) (i32.const 1))")
        .Comment("i + 1")
        .Code("(local.set $i)").Comment("i = i + 1")
        .Code("(br $while)").Comment("continue")
        .Indent(-2)
        .Code(")").Indent(-2)
        .Code(")").CommentLine("")
        .Code("(local.get $len)").Comment("return len")
        .Indent(-2)
        .Code(")").CommentLine("");
    
    assert(original == indent);
    control.indent = original;  // Set it back to it's originl just for good measure
}