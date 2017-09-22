version=$1
spec=$2
sed -i "s/[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+/$version/g" $spec
