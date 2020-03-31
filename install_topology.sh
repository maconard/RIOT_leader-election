if [ -z "$1" ] 
then
    echo "Filename argument not supplied"
    exit 1
fi

mv $1 ../../dist/tools/desvirt/desvirt/.desvirt/$1
echo "$1 has been installed for desvirt to use"

