/*
 *  git2r, R bindings to the libgit2 library.
 *  Copyright (C) 2013-2014 The git2r contributors
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2,
 *  as published by the Free Software Foundation.
 *
 *  git2r is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "git2.h"
#include "git2r_error.h"
#include "git2r_index.h"
#include "git2r_repository.h"

/**
 * Add files to a repository
 *
 * @param repo S4 class git_repository
 * @param path
 * @return R_NilValue
 */
SEXP git2r_index_add(SEXP repo, SEXP path)
{
    int err;
    git_index *index = NULL;
    git_repository *repository = NULL;

    if (git2r_error_check_string_arg(path))
        error("Invalid arguments to git2r_index_add");

    repository= git2r_repository_open(repo);
    if (!repository)
        error(git2r_err_invalid_repository);

    err = git_repository_index(&index, repository);
    if (err < 0)
        goto cleanup;

    err = git_index_add_bypath(index, CHAR(STRING_ELT(path, 0)));
    if (err < 0)
        goto cleanup;

    err = git_index_write(index);
    if (err < 0)
        goto cleanup;

cleanup:
    if (index)
        git_index_free(index);

    if (repository)
        git_repository_free(repository);

    if (err < 0)
        error("Error: %s\n", giterr_last()->message);

    return R_NilValue;
}

/**
 * Add or update index entries matching files in the working
 * directory.
 *
 * @param repo S4 class git_repository
 * @param path array of path patterns
 * @return R_NilValue
 */
SEXP git2r_index_add_all(SEXP repo, SEXP path)
{
    int err;
    size_t i, len;
    const char* err_msg = NULL;
    git_strarray pathspec = {0};
    git_index *index = NULL;
    git_repository *repository = NULL;

    if (R_NilValue == path || !isString(path))
        error("Invalid arguments to git2r_index_add_all");

    repository= git2r_repository_open(repo);
    if (!repository)
        error(git2r_err_invalid_repository);

    /* Count number of non NA values */
    len = length(path);
    for (i = 0; i < len; i++)
        if (NA_STRING != STRING_ELT(path, i))
            pathspec.count++;

    /* We are done if no non-NA values  */
    if (!pathspec.count)
        goto cleanup;

    /* Allocate the strings in pathspec */
    pathspec.strings = malloc(pathspec.count * sizeof(char*));
    if (!pathspec.strings) {
        err = -1;
        err_msg = git2r_err_alloc_memory_buffer;
        goto cleanup;
    }

    /* Populate the strings in pathspec */
    for (i = 0; i < pathspec.count; i++)
        if (NA_STRING != STRING_ELT(path, i))
            pathspec.strings[i] = CHAR(STRING_ELT(path, i));

    err = git_repository_index(&index, repository);
    if (err < 0)
        goto cleanup;

    err = git_index_add_all(index, &pathspec, 0, NULL, NULL);
    if (err < 0)
        goto cleanup;

    err = git_index_write(index);
    if (err < 0)
        goto cleanup;

cleanup:
    if (pathspec.strings)
        free(pathspec.strings);

    if (index)
        git_index_free(index);

    if (repository)
        git_repository_free(repository);

    if (err < 0) {
        if (err_msg)
            error(err_msg);
        else
            error("Error: %s\n", giterr_last()->message);
    }

    return R_NilValue;
}