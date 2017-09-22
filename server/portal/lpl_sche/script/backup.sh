NGINX_PATH=/opt/interlive/nginx
DATE=$(date +%y%m%d)
BACKUP_DIR=conf.$DATE 
if [ ! -d $NGINX_PATH/$BACKUP_DIR ];
then
mkdir $NGINX_PATH/$BACKUP_DIR
fi
cp $NGINX_PATH/conf/* $NGINX_PATH/$BACKUP_DIR/ -r
echo "Success" 
