#!/bin/sh

envsubst '${FRONTEND_HOST} ${BACKEND_HOST}' \
  < /etc/nginx/nginx.conf.template \
  > /etc/nginx/conf.d/default.conf

exec nginx -g 'daemon off;'