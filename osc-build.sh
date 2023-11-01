#!/bin/bash

set -x

[ -z "$MTR_GITLAB_LOGIN" ] && exit 2
[ -z "$MTR_GITLAB_HOST" ] && exit 3
[ -z "$MTR_GITLAB_PASSWORD" ] && exit 4

if [ ! -f ./github_token ]; then
	if [ -z "$GITHUB_TOKEN ]; then
		echo "missing mandatory github_token"
		exit 5
	else
		echo $GITHUB_TOKEN > ./github_token
	fi
fi

DOCKER_CRE=podman # or docker
COMMITID=$(git rev-parse --short HEAD)
#COMMITDATE=$(git show -s --format=%cd --date=format:'%d%m%Y' $COMMITID)
COMMITDATE=$(date +%d%m%Y)
MTRPREFIX="osc/onmetal"
# Component name (for publishing in MTR)
CNAME='dp-service'

#--secret=id=github_token,src=../../github_token
$DOCKER_CRE build --platform=linux/amd64 --build-arg="DPSERVICE_FEATURES=-Denable_virtual_services=true" --secret=id=github_token,src=./github_token -t $MTRPREFIX/$CNAME:$COMMITDATE-$COMMITID .

echo "${MTR_GITLAB_PASSWORD}" | $DOCKER_CRE login --username "${MTR_GITLAB_LOGIN}" --password-stdin "${MTR_GITLAB_HOST}"

$DOCKER_CRE tag $MTRPREFIX/$CNAME:$COMMITDATE-$COMMITID "$MTR_GITLAB_HOST/$MTRPREFIX/$CNAME:$COMMITDATE-$COMMITID"
$DOCKER_CRE push -q "$MTR_GITLAB_HOST/$MTRPREFIX/$CNAME:$COMMITDATE-$COMMITID"

# uncomment this if you want to reconfigure MTR image as public; then also MTR_GITLAB_TOKEN needs to be defined
#curl -H "Authorization: Bearer $MTR_GITLAB_TOKEN" -H 'Content-Type: application/json' -XPOST https://${MTR_GITLAB_HOST}/api/v1/repository/${target_basename}/changevisibility -d '{"visibility": "public"}'



