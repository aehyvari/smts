  #!/bin/bash

if [ $# != 1 ]; then
    echo "Usage: $0 <provide shared bucket dir>"
    exit 1
fi
for folder in "$1"/*; do
    for file in "$folder"; do
      time ./SMTS/opensmt/build/src/bin/opensmt $file
     #/usr/bin/time -o ${file}.time ./build/src/bin/opensmt $file
    done
done
