#!/bin/bash

function msg()
{
    echo -e "\033[32m${1}\033[0m"
}

function compile() {
clang -emit-llvm -c simple_prog.c
clang -emit-llvm -S simple_prog.c #for llvm ir
}

compile


msg "Run SimpleTests on Rust LLVM code"
/opt/llvm/build/bin/opt  -load  ../build/SimpleAliasTest/libSimpleAliasTest.so  -simpletester simple_prog.bc > /dev/null
