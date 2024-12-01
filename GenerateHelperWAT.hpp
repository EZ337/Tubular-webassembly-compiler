#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include "Control.hpp"
#pragma once

using std::string;


#include <iostream>
#include <string>
#include <sstream>
#include <cstdarg>  // For va_list

// Function to generate the WAT function header
// Credits to OpenAI ChatGPT. Modified by me
void GenerateFunctionHeader(Control& control, std::string func_name, std::string func_return, ...) {
    va_list args;
    va_start(args, func_return);

    // Start building the result string for the function header
    std::stringstream resultString;
    resultString << "(func $" << func_name;  // Function name

    // Handle the parameters
    const char* param;
    while ((param = va_arg(args, const char*)) != nullptr) {
        // Get the WAT type from the next argument
        resultString << " (param $" << param << ")";
    }

    // Add the return type
    if (func_return != "")
        resultString << " (result " << func_return << ")";

    // End argument list
    va_end(args);

    // Output the result string
    control.Code(resultString.str())
        .Indent(2)
        .CommentLine("Variables");
}

void GenerateSizeFunction(Control& control)
{
    int original = control.indent;

    // emplex::Token size_function{emplex::Lexer::ID_SIZE, "size", 0,0};
    // control.symbols.AddFunction(size_function, {Type("string")}, Type("int"));

    control.CommentLine("Function to get the size of a string")
        .Code("(func $size (param $str i32) (result i32)")
        .Indent(2)
        .CommentLine("Variables")
        .Code("(local $len i32)")
        .Code("(local $i i32)")
        .CommentLine("")
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
        .Code("(loop $while").Indent(2)
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
        .Code(")").Indent(-2).CommentLine("")
        .Code("(local.get $len)").Comment("return len")
        .Indent(-2)
        .Code(")");
    
    control.indent = original;  // Set it back to it's originl just for good measure
    
    control.Code("(export \"size\" (func $size))")
        .CommentLine("");

}

void GenerateStrCpy(Control& control)
{
    GenerateFunctionHeader(control, "_strcpy", "i32", "str i32", "dest i32", "amount i32", nullptr);
    
    control.Code("(local $i i32)")
        .CommentLine().CommentLine("Begin Code")

        // Function body
        .Code("(local.set $i (i32.const 0))").Comment("set loop counter to 0")
        .CommentLine("Setup while")
        .Code("(block $exit_while").Indent(2)
        .Code("(loop $while").Indent(2)
        .Code("(local.get $i)")
        .Code("(local.get $amount)")
        .Code("(i32.ge_s)").Comment("continue if i < amount")
        .Code("(br_if $exit_while)").Comment("break if i >= amount")

        .CommentLine("While body")
        .Code("(i32.add (local.get $dest) (local.get $i))").Comment("get the dest location")
        .Code("(i32.add (local.get $str) (local.get $i))").Comment("get addr of str[i]")
        .Code("(i32.load8_u)").Comment("Dereference str[i]")
        .Code("(i32.store8)").Comment("Store")

        .CommentLine()
        .CommentLine("Increment i")
        .Code("(i32.add (local.get $i) (i32.const 1))")
        .Code("(local.set $i)").Comment("set i to i+1")
        .Code("(br $while)")
        
        // Close while
        .Indent(-2).Code(')')
        .Indent(-2).Code(')')
        .Code("(local.get $dest)").Comment("Return the starting index of new str")
        
        // Closing
        .Indent(-2)
        .Code(")").CommentLine("");
}

void GenerateStrConcat(Control& control)
{
    int original = control.indent;

    GenerateFunctionHeader(control, "_str_concat", "i32", "str1 i32", "str2 i32", nullptr);

        control.Code("(local $size1 i32)").Comment("str1.size")
        .Code("(local $size2 i32)").Comment("str2.size")
        .Code("(local $newPos i32)").Comment("location of concatenated string")
        .CommentLine("")

        .CommentLine("Code Begin")  // Starting code
        .Code("(local.set $size1 (call $size (local.get $str1)))")
        .Comment("size1 = size(str1)")
        .Code("(local.set $size2 (call $size (local.get $str2)))")
        .Comment("size2 = size(str2)")
        .Code("(i32.add (local.get $size1) (local.get $size2))")
        .Code("(i32.const 1)").Comment("Add a 1 for the nullpos")
        .Code("(i32.add)").Comment("size1+size2+1")
        .Code("(local.set $newPos (call $_alloc_str))")
        .Comment("(newPos = allocated_pos)")
        .CommentLine()

        .CommentLine("copy str1")
        .Code("(local.get $str1)").Comment("str to copy")
        .Code("(local.get $newPos)").Comment("pos to copy to")
        .Code("(local.get $size1)").Comment("amount to copy (size of str1)")
        .Code("(call $_strcpy)")
        
        .CommentLine("Now copy str2")
        .Code("(local.get $size1)").Comment("Offset")
        .Code("(i32.add)").Comment("pos to copy to")
        .Code("(local.get $str2)").Comment("str2 copy")
        // .Code("(local.get $newPos)").Comment("Get startPos")
        .Code("(call $_i32swap)").Comment("Swap dest+offset with str2")
        .Code("(local.get $size2)").Comment("amount to copy (size of str2)")
        .Code("(call $_strcpy)")
        .Drop().Comment("We don't want the start of the seconod string")

        .CommentLine()
        .Code("(local.get $newPos)").Comment("return newStr pos")
        .Indent(-2)
        .Code(")").CommentLine("");
    
    control.indent = original;  // Set it back to it's originl just for good measure
    // control.Code("(export \"Concat\" (func $_str_concat))");
    control.CommentLine("");
};

void GenerateCharToString(Control& control)
{
    GenerateFunctionHeader(control, "_char_to_string", "i32", "char i32", nullptr);

    control.Code("(local $pos i32)").Comment("The position of the allocated char")
        .CommentLine().CommentLine("Begin Code")
        .Code("(call $_alloc_str (i32.const 2))").Comment("one for char, one for nullterm")
        .Code("(local.set $pos)").Comment("Sets pos to the allocated str for return")
        .Code("(local.get $pos)").Comment("gets the position to store.")
        .Code("(i32.store8 (local.get $pos) (local.get $char))").Comment("store $char at pos")
        
        .CommentLine("pos is already on the stack so return")
        .Indent(-2).Code(')').CommentLine();
}

void GenerateI32Swap(Control& control)
{
    control.CommentLine("Function to swap top 2 items on the stack. (both i32 version)");
    GenerateFunctionHeader(control, "_i32swap", "i32 i32", "first i32", "second i32", nullptr);
        
    control.CommentLine("Now place them down in reverse order")
        .Code("(local.get $second)")
        .Code("(local.get $first)")
        .Indent(-2).Code(")").CommentLine();
}
