#!/bin/bash

# This script is not guaranteed to work in all environments without
# customization. Meant primarily to show the concept of installing a
# new pipeline

export ROOT_DIR=/usr/local/webos
export PIPELINE=umediapipeline
export PATH=/usr/local/webos/usr/bin:$PATH

# Copy the pipeline executable
cp ${ROOT_DIR}/usr/sbin/${PIPELINE} ${ROOT_DIR}/var/palm/ums/pipelines/umediapipeline_a

# Create a new roles file for the pipeline. Correspondingly there will
# also need to be a roles file for any potential clients.
cat > ${ROOT_DIR}/var/palm/ls2/roles/prv/com.webos.umediapipeline_a.json <<EOF
{
    "role": {
        "exeName":"${ROOT_DIR}/var/palm/ums/pipelines/umediapipeline_a",
        "type": "regular",
        "allowedNames": ["com.webos.pipeline.*",
	"com.webos.rm.client.*"]
    },
    "permissions": [
        {
            "service":"com.webos.pipeline.*",
            "inbound":["com.webos.pipelinectrl.*"],
            "outbound":["com.webos.pipelinectrl.*"]
        },
        {
            "service":"com.webos.rm.client.*",
            "inbound":["com.webos.media"],
            "outbound":["com.webos.media"]
        }
    ]
}
EOF

# Tell Luna to rescan services
ls-control scan-services

# Create a pipeline uMS config file for the new pipeline
cat > ${ROOT_DIR}/var/lib/ums/conf/a.conf <<EOF
version = "1.0";

pipelines = (
	{
		# REFERENCE pipeline type. ONLY used for internal uMS dev
		type = "refa";
		name = "REFRENCE video pipeline.";
		bin  = "${ROOT_DIR}/var/palm/ums/pipelines/umediapipeline_a";
		priority = 4;

		# maximum number of restarts in a 5 second interval
		# set to 0 to disable restarts
		# if left out it defaults to 0
		max_restarts = 2;

		environment = (
			{
				name = "LD_LIBRARY_PATH";
				value = "some_lib_path";
				op = "PREPEND";
			},
			{
				name = "PATH";
				value = "some_bin_path";
				op = "APPEND";
			},
			{
				name = "SOME_OTHER_VARIABLE";
				value = "some_value";
				op = "REPLACE";
			}
		      );
	}
)
EOF


