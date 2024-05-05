#!/bin/bash
# Script to create a Linux group and add users

group_name=$1

# Check if group already exists
if getent group "$group_name" > /dev/null; then
    echo "Group $group_name already exists."
else
    # Using sudo to create the group
    if ! sudo groupadd "$group_name"; then
        echo "Failed to create group $group_name."
        exit 1
    fi
fi

shift
all_success=true
for username in "$@"; do
    # Using sudo to modify the user group
    if ! sudo usermod -aG "$group_name" "$username"; then
        echo "Failed to add $username to $group_name."
        all_success=false
    fi
done

if $all_success; then
    echo "Group $group_name created or updated and users added successfully."
    exit 0
else
    echo "Failed to add one or more users to $group_name."
    exit 2
fi
