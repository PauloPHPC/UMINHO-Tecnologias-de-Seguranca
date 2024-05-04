#!/bin/bash
username=$1
password=$2
sudo useradd -m "$username" -p "$password"