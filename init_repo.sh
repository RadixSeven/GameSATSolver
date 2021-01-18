#!/bin/bash

# Go to root of repo
cd $(git rev-parse --show-toplevel)

# Check whether this has already run successfully
REPO_IS_READY=repo_is_ready
if [[ -f $REPO_IS_READY ]]; then
    echo "Repo was already initialized."
fi

if (echo a version 2.9.0; git --version) | sort -Vk3 | tail -1 | grep -q git
then
    # Has hooksPath - set it
    git config core.hooksPath hooks
else
    # Does not have hooksPath. Symlink all hooks from .git/hooks
    for f in ./hooks/*; do 
        target=.git/hooks/"$(basename -- $f)"
        if [[ -e "$target" ]]; then
            echo "$target already exists please remove it to allow "
            echo "completing setup"
            exit 1
        fi
        ln -s $f $target
    done
fi

touch $REPO_IS_READY

