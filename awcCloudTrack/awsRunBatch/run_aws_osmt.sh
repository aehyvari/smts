  #!/bin/bash

if [ $# != 1 ]; then
    echo "Usage: $0 <provide shared bucket file instance>"
    exit 1
fi

echo $1
ulimit -St 1200; time ./SMTS/opensmt/build/src/bin/opensmt $1
     #/usr/bin/time -o ${file}.time ./build/src/bin/opensmt $file
