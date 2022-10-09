Title: Account Signals
Slug: account-signals

## Account Signals

### Signal List

* [account-created](#account-created)
* [account-destroying](#account-destroying)
* [account-disabled](#account-disabled)
* [account-enabled](#account-enabled)
* [account-status-changed](#account-status-changed)
* [account-actions-changed](#account-actions-changed)
* [account-error-changed](#account-error-changed)
* [account-signed-on](#account-signed-on)
* [account-signed-off](#account-signed-off)

### Signal Details

#### account-created

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when an account is created by calling purple_account_new.

**Parameters:**

**account**
: The account.

**user_data**
: User data set when the signal handler was connected.

----

#### account-destroying

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when an account is about to be destroyed.

**Parameters:**

**account**
: The account.

**user_data**
: User data set when the signal handler was connected.

----

#### account-disabled

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when an account is disabled.

**Parameters:**

**account**
: The account that was disabled.

**user_data**
: User data set when the signal handler was connected.

----

#### account-enabled

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when an account is enabled.

**Parameters**:

**account**
: The account that was enabled.

**user_data**
: User data set when the signal handler was connected.

----

#### account-status-changed

```c
void user_function(PurpleAccount *account,
                   PurpleStatus *old,
                   PurpleStatus *new,
                   gpointer user_data);
```

Emitted when the status of an account changes (after the change).

**Parameters:**

**account**
: The account that changed status.

**old**
: The status before change.

**new**
: The status after change.

**user_data**
: User data set when the signal handler was connected.

----

#### account-actions-changed

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when the account actions are changed after initial connection.

**Parameters:**

**account**
: The account whose actions changed.

**user_data**
: User data set when the signal handler was connected.

----

#### account-error-changed

```c
void user_function(PurpleAccount *account,
                   const PurpleConnectionErrorInfo *old_error,
                   const PurpleConnectionErrorInfo *current_error,
                   gpointer user_data);
```

Emitted when `account`'s error changes.  You should not call purple_account_clear_current_error() while this signal is being emitted.

**Parameters:**

**account**
: The account whose error has changed.

**old_error**
: The account's previous error, or `NULL` if it had no error.  After this signal is emitted, `old_error` is not guaranteed to be a valid pointer.

**new_error**
: The account's new error, or `NULL` if it has no error. If not `NULL`, `new_error` will remain a valid until pointer just after the next time this signal is emitted for this `account`. See `purple_account_get_current_error()`.

**user_data**
: User data set when the signal handler was connected.

----

#### account-signed-on

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when an account has signed on.

**Parameters:**

**account**
: The account that has signed on.

**user_data**
: User data set when the signal handler was connected.

----

#### account-signed-off

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when an account has signed off.

**Parameters:**

**account**
: The account that has signed off.

**user_data**
: User data set when the signal handler was connected.
