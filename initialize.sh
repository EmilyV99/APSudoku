#!/bin/sh

# Move to script directory.
cd "$(dirname "$0")";
SRC_DIR=`pwd`;

# Create a nested folder and moves into it.
declare -a dirs=("build" "allegro5" "deps");
for dir in "${dirs[@]}"; do
        dir=${dir/#/"./"}
        if [ ! -d "$dir" ]; then
                mkdir $dir;
        fi
        cd $dir;
done

# Check if NuGet is installed, then download and set up Allegro.
if ! command -v nuget &> /dev/null
then
    echo "Please install the NuGet CLI to download dependencies."
    exit 1
fi
echo "Checking for NuGet package 'AllegroDeps':";
nuget install AllegroDeps -Version 1.14.0 -OutputDirectory . -ExcludeVersion;
cp -rf ./AllegroDeps/build/native/include ./include;
cp -rf ./AllegroDeps/build/native/v143/x64/deps/lib ./lib;

# Start the build.
cd $SRC_DIR;
cd ./build;
echo "Running CMake:";
cmake ..;
