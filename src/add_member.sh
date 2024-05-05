#!/bin/bash

# Check if two arguments are provided
if [ $# -ne 2 ]; then
    echo "Usage: $0 username group_name"
    exit 1
fi

username=$1
group_name=$2

# Add the user to the group
if sudo usermod -aG "$group_name" "$username"; then
    echo "User $username successfully added to group $group_name."
    exit 0
else
    echo "Failed to add user $username to group $group_name."
    exit 1
fi
