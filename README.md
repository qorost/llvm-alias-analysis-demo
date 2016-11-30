# llvm-alias-analysis-demo
A small project to use llvm alias analysis


## Compile

### Compile LLVM 

Follow the instruction on http://llvm.org/docs/CMake.html. In my case, I put the code in /opt/llvm directory.

```
cd /opt/llvm/
mkdir build
cd build
#build
cmake /opt/llvm
cmake --build .
```

### Set Environment Variable for LLVM 

```
export LLVM_DIR="/opt/llvm/build/lib/cmake/llvm"
export PATH=${PATH}:"/opt/llvm/build/bin"
```

### Compile this project

```
mkdir build
cd build
cmake ../src/
```


### Run

There is a `run.sh` under the `test` directory. Go and run it.
