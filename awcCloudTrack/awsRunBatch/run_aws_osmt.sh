  #!/bin/bash

if [ $# != 1 ]; then
    echo "Usage: $0 <provide shared bucket dir>"
    exit 1
fi
for folder in "$1"/*; do
    for file in "$folder"; do
      echo $file
      ulimit -St 1200; time ./SMTS/opensmt/build/src/bin/opensmt $file
      sleep 60
     #/usr/bin/time -o ${file}.time ./build/src/bin/opensmt $file
    done
done
