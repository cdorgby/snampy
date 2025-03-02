#!/bin/bash

NAT='0|[1-9][0-9]*'
RELEASE_BRANCH_REGEX="\
^release-($NAT)\\.($NAT)\\.($NAT)$"
_script_dir="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

_dry_run=0
_create_release_tag=0
_create_release_branch=0

_in_git_repo=`git rev-parse --is-inside-work-tree 2>/dev/null`

show_help() {
    echo "$0: [options] <command>"
    echo "Commands:"
    echo -e "\tget_version \t\t- Print current release version"
    echo -e "\tget_build_version\t- Print current release version, suitable for integration into the build"
    echo -e "\tget_branch\t\t- Get name of current branch"
    echo -e ""
    echo -e "\tbump_major\t\t- Bump major porition of the version string and display"
    echo -e "\tbump_minor\t\t- Bump minor portion of the version string and display"
    echo -e "\tbump_patch\t\t- Bump path portion of the version string and display"
    echo -e "\tbump_rc\t\t\t- Add/Bump Release Candiate version string."
    echo -e "\tfinalize_release\t-"
    echo -e "Options:"
    echo -e "\t-d \t- Dry-run, don't modify git repository"
    echo -e "\t-b \t- Create release branch in the git repo"
    echo -e "\t-t \t- Create release tag in the git repo"

}

git_build_last_tag() {
    #declare -l lasttag=`git name-rev --tags --name-only $(git rev-parse HEAD)`
    if [ "$_in_git_repo" = "true" ]; then
        declare -l lasttag=`git describe --abbrev=0 2>/dev/null`
    else
        declare -l lasttag="0.1.0"
    fi

    if [ "$lasttag" = "undefined" -o -z "$lasttag" ]; then
        lasttag="0.1.0"
    fi
    echo $lasttag
}

git_current_version() {
    #declare -l ver=$(git tag --sort=-committerdate | head -1)
    if [ "$_in_git_repo" = "true" ]; then
        declare -l ver=$($_script_dir/git_closest_tag.py)
    else
        declare -l ver="v0.1.0"
    fi

    echo $ver
}
git_prev_version() {
    if [ "$_in_git_repo" = "true" ]; then
        declare -l ver=$(git_current_version)
        declare -l prev_ver=$(git describe --tags --abbrev=0 $(git rev-list $ver) 2>/dev/null | head -2 | awk '{split($0, tags, "\n")} END {print tags[1]}')
    else
        declare -l prev_ver="v0.1.0"
    fi
    echo $prev_ver
}

git_current_commit() {
    if [ "$_in_git_repo" = "true" ]; then
        declare -l hash=$(git rev-list HEAD --max-count=1 --abbrev-commit)
    else
        declare -l hash="0000000"
    fi

    echo $hash
}

git_change_log() {
    declare -l cur_ver=$(git_current_version)
    declare -l prev_ver=$(git_prev_version)

    if [ "$_in_git_repo" = "true" ]; then
        declare -l changes=$(git log --pretty="- %s" $cur_ver...$prev_ver)
    else
        declare -l changes="No changes"
    fi
    declare -l build_ver=$(git_build_version)

    printf "# 🎁 Release notes (\`$cur_ver\`)\n\n## Changes\n$changes\n\n## Metadata\n\`\`\`\nThis version     : $cur_ver\nPrevious version : $prev_ver\nDev version      : $build_ver\nTotal commits    : $(echo "$changes" | wc -l)\n\`\`\`\n"
}

git_branch() {
    declare -l branchname=`git symbolic-ref --short -q HEAD`

    echo $branchname
}

get_current_version() {
    declare -l ver=$(git_current_version)

    if [ -z "$ver" ]; then
        ver="v0.1.0"
    else
        if [ "`$_script_dir/semver.sh validate $ver`" = "invalid" ]; then
            ver="v0.1.0"
        fi
    fi

    echo $ver
}

git_build_version() {
    declare -l ver=$(get_current_version)
    declare -l ahead=$(git_get_ahead $ver)
    declare -l hashish=$(git_current_commit)

    if [ $ahead -gt 0 ]; then
        echo v$ver+$ahead-$hashish | sed -e 's/^v//'
    else
        echo v$ver | sed -e 's/^v//'
    fi
}

get_current_branch() {
    declare -l ver=$(git_branch)

    echo $ver
}

validate_branch() {
    declare -l branch=$(get_current_branch)

    if [[ "$branch" =~ $RELEASE_BRANCH_REGEX ]]; then
        echo $branch
        return 0
    fi

    echo "$branch is not a valid release branch name."
    exit 1
}

create_build_tag() {

    if [ "$_in_git_repo" = "true" ]; then

        if [ "$_dry_run" = "0" ]; then
            git tag -a -m "Release tag: $1" "v$1"
            [ $? = 0 ] || exit $?
        fi
        echo "Don't forget to run: git push --tags"
    else
        echo "Not in a GIT repository"
    fi
}

git_get_ahead() {


    if [ "$_in_git_repo" = "true" ]; then
        git rev-list --left-right --count $1...HEAD | cut -f 2
    else
        echo "0"
    fi
}

git_get_behind() {

    if [ "$_in_git_repo" = "true" ]; then
        git rev-list --left-right --count $1...HEAD | cut -f 1
    else
        echo "0"
    fi
}

create_build_branch() {
    declare -l ver=$1

    declare -l branch=$(get_current_branch)

    if [ "$branch" != "main" ]; then
        if [[ ! "$branch" =~ $RELEASE_BRANCH_REGEX ]]; then
            echo "You must be on the main or on branch to create a release-branch"
            exit 1
        fi
    fi

    if [ "$(git_get_ahead $branch)" != "0" ]; then
        echo "Current HEAD is $(git_get_ahead $branch) ahead of main "
        exit 1
    fi

    if [ "$(git_get_behind $branch)" != "0" ]; then
        echo "Current HEAD is $(git_get_behind $branch) commit(s) behind of main "
        exit 1
    fi


    if [ "$_dry_run" = "0" ]; then
        git branch "release-$ver"
        [ $? = 0 ] || exit $?
    fi
    echo "Don't forget to run: git push origin release-$ver:release-$ver"
}

do_bump() {

    if [ "$_create_release_tag" == "1" ]; then
        validate_branch
        create_build_tag "$1"
    elif [ "$_create_release_branch" == "1" ]; then
        create_build_branch "$1"
        new_ver=$($_script_dir/semver.sh bump prerel $1)
        create_build_tag "$new_ver"
    else
        echo "$1"
    fi
}

bump_major() {
    declare -l ver=$(get_current_version)

    declare -l new_ver=$($_script_dir/semver.sh bump major $ver)
    if [ "$_create_release_tag" == "1" ]; then
        new_ver=$($_script_dir/semver.sh bump prerel $new_ver)
    fi

    do_bump $new_ver
}

bump_minor() {
    declare -l ver=$(get_current_version)

    declare -l new_ver=$($_script_dir/semver.sh bump minor $ver)
    if [ "$_create_release_tag" == "1" ]; then
        new_ver=$($_script_dir/semver.sh bump prerel $new_ver)
    fi

    do_bump $new_ver
}

bump_patch() {
    declare -l ver=$(get_current_version)

    declare -l new_ver=$($_script_dir/semver.sh bump patch $ver)
    if [ "$_create_release_tag" == "1" ]; then
        new_ver=$($_script_dir/semver.sh bump prerel $new_ver)
    fi

    do_bump $new_ver
}

bump_rc() {
    declare -l ver=$(get_current_version)

    declare -l new_ver=$($_script_dir/semver.sh bump prerel $ver)
    if [ "$_create_release_branch" == "1" ]; then
        echo "A release-branch is not supposed to have -rc version"
        exit 1
    fi

    if [[ "$ver" =~ "^v($NAT)\\.($NAT)\\.($NAT)$" ]]; then
        echo "The current version is final, can't bump Release-Candidate."
        exit 1
    fi
    do_bump $new_ver
}

# Reset, just in case
OPTIND=1

while getopts "h?vtdb" opt; do
  case "$opt" in
    h|\?)
      show_help
      exit 0
      ;;
    d)  _dry_run=1 ;;
    t)  _create_release_tag=1 ;;
    b)  _create_release_branch=1 ;;
  esac
done

shift $((OPTIND-1))
[ "${1:-}" = "--" ] && shift

if [ "$_create_release_tag" == "1" -a "$_create_release_branch" == "1" ]; then
    echo "Either create a release branch, or a release tag. Can't do both"
    exit 1
fi
_cmd=$1
shift


function cleanup {
  echo "Something failed, clean the repository"
}
trap cleanup EXIT

case "$_cmd" in
    get_br*)
        get_current_branch
        ;;
    get_ver*)
        get_current_version
        ;;
    get_build_v*)
        git_build_version
        ;;
    bump_major)
        bump_major
        ;;
    bump_minor)
        bump_minor
        ;;
    bump_patch)
        bump_patch
        ;;
    bump_rc)
        bump_rc
        ;;
    release_n*)
        git_change_log
        ;;
    finalize_release)
        finalize_release
        ;;
esac

trap - EXIT

exit 0
