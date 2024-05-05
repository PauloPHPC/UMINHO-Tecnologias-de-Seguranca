#!/bin/bash

# Script to delete a Linux group

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 group_name"
    exit 1
fi

group_name=$1

# Delete the group
if groupdel "$group_name"; then
    echo "Group '$group_name' successfully deleted."
    exit 0
else
    echo "Failed to delete group '$group_name'."
    exit 1
fi