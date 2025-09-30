
. ./AtlasNetVars.sh   

if [ $# -lt 1 ]; then
    echo "Usage: $0 <GameServerExeName>"
    exit 1
fi

Setup.sh $1
Start.sh God