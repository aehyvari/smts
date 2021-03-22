#!/bin/bash

# smts -> server
-np 1 python3 SMTS/server/smts.py  -l
# smts -> opensmt clients
-np ${AWS_BATCH_JOB_NUM_NODES} SMTS/build/solver_opensmt -s ${ip}:3000




