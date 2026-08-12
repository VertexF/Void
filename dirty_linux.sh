echo "IMPORTANT! The assets don't come with the source code."
mkdir build
read -p "Debug or release? [d/r]" version && [[ $version == [dD] || $version == [rR] ]]

if [ $version == r ]; then
    cmake -G "Unix Makefiles" -S . -B build -DCMAKE_BUILD_TYPE=Release
elif [ $version == d ]; then
    cmake -G "Unix Makefiles" -S . -B build -DCMAKE_BUILD_TYPE=Debug
fi
