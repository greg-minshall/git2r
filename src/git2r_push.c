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

#include <Rdefines.h>
#include "git2.h"

#include "git2r_arg.h"
#include "git2r_cred.h"
#include "git2r_error.h"
#include "git2r_push.h"
#include "git2r_repository.h"
#include "git2r_signature.h"

/**
 * The invoked callback on each status entry
 *
 * @param ref :TODO:DOCUMENTATION:
 * @param msg :TODO:DOCUMENTATION:
 * @param data :TODO:DOCUMENTATION:
 * @return 0
 */
static int git2r_push_status_foreach_cb(
    const char *ref,
    const char *msg,
    void *data)
{
    const char **msg_dst = (const char **)data;
    if (msg != NULL && *msg_dst == NULL)
        *msg_dst = msg;
    return 0;
}

/**
 * Check if any non NA refspec
 *
 * @param refspec The string vector of refspec to push
 * @return 1 if nothing to push else 0
 */
static int git2r_nothing_to_push(SEXP refspec)
{
    size_t i, n;

    n = length(refspec);
    if (0 == n)
        return 1; /* Nothing to push */

    for (i = 0; i < n; i++) {
        if (NA_STRING != STRING_ELT(refspec, i))
            return 0;
    }

    /* Nothing to push */
    return 1;
}

/**
 * Push
 *
 * @param repo S4 class git_repository
 * @param name The remote to push to
 * @param refspec The string vector of refspec to push
 * @param credentials The credentials for remote repository access.
 * @param msg The one line long message to be appended to the reflog
 * @param who The identity that will used to populate the reflog entry
 * @return R_NilValue
 */
SEXP git2r_push(
    SEXP repo,
    SEXP name,
    SEXP refspec,
    SEXP credentials,
    SEXP msg,
    SEXP who)
{
    int err;
    size_t i, n;
    const char *err_msg = NULL;
    git_signature *signature = NULL;
    git_push *push = NULL;
    git_remote *remote = NULL;
    git_repository *repository = NULL;
    git_remote_callbacks callbacks = GIT_REMOTE_CALLBACKS_INIT;

    if (git2r_arg_check_string(name)
        || git2r_arg_check_string_vec(refspec)
        || git2r_arg_check_credentials(credentials)
        || git2r_arg_check_string(msg)
        || git2r_arg_check_signature(who))
        Rf_error("Invalid arguments to git2r_push");

    if (git2r_nothing_to_push(refspec))
        return R_NilValue;

    repository = git2r_repository_open(repo);
    if (!repository)
        Rf_error(git2r_err_invalid_repository);

    err = git2r_signature_from_arg(&signature, who);
    if (err < 0)
        goto cleanup;

    err = git_remote_load(&remote, repository, CHAR(STRING_ELT(name, 0)));
    if (err < 0)
        goto cleanup;

    callbacks.credentials = &git2r_cred_acquire_cb;
    callbacks.payload = credentials;
    err = git_remote_set_callbacks(remote, &callbacks);
    if (err < 0)
        goto cleanup;

    err = git_remote_connect(remote, GIT_DIRECTION_PUSH);
    if (err < 0)
        goto cleanup;

    err = git_push_new(&push, remote);
    if (err < 0)
        goto cleanup;

    n = length(refspec);
    for (i = 0; i < n; i++) {
        if (NA_STRING != STRING_ELT(refspec, i)) {
            err = git_push_add_refspec(push, CHAR(STRING_ELT(refspec, i)));
            if (err < 0)
                goto cleanup;
        }
    }

    err = git_push_finish(push);
    if (err < 0)
        goto cleanup;

    err = git_push_unpack_ok(push);
    if (err < 0)
        goto cleanup;

    err = git_push_status_foreach(push, git2r_push_status_foreach_cb, &err_msg);
    if (err < 0)
        goto cleanup;
    if (err_msg != NULL) {
        err = -1;
        goto cleanup;
    }

    err = git_push_update_tips(push, signature, CHAR(STRING_ELT(msg, 0)));
    if (err < 0)
        goto cleanup;

cleanup:
    if (signature)
        git_signature_free(signature);

    if (push)
        git_push_free(push);

    if (remote) {
        if (git_remote_connected(remote))
            git_remote_disconnect(remote);
        git_remote_free(remote);
    }

    if (repository)
        git_repository_free(repository);

    if (err < 0) {
        if (NULL != err_msg)
            Rf_error("Error: %s\n", err_msg);
        else
            Rf_error("Error: %s\n", giterr_last()->message);
    }

    return R_NilValue;
}
