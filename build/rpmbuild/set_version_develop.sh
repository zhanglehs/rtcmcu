branch=$1
version=$2
spec=$3

sed -i "s/[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+/$version/g" $spec 
sed -i "s/[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+/$branch/g" $spec 

