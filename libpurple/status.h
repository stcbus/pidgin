/**
 * @file status.h Status API
 * @ingroup core
 */
/*
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#ifndef _PURPLE_STATUS_H_
#define _PURPLE_STATUS_H_

#define PURPLE_TYPE_STATUS             (purple_status_get_type())
#define PURPLE_STATUS(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_STATUS, PurpleStatus))
#define PURPLE_STATUS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_STATUS, PurpleStatusClass))
#define PURPLE_IS_STATUS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_STATUS))
#define PURPLE_IS_STATUS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_STATUS))
#define PURPLE_STATUS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_STATUS, PurpleStatusClass))

/** @copydoc _PurpleStatus */
typedef struct _PurpleStatus           PurpleStatus;
/** @copydoc _PurpleStatusClass */
typedef struct _PurpleStatusClass      PurpleStatusClass;

#define PURPLE_TYPE_STATUS_TYPE        (purple_status_type_get_type())

/**
 * PurpleStatusType's are created by each protocol.  They outline the
 * available statuses of the protocol.  AIM, for example, supports
 * an available state with an optional available message, an away
 * state with a mandatory message, and an invisible state (which is
 * technically "independent" of the other two, but we'll get into
 * that later).  PurpleStatusTypes are very permanent.  They are
 * hardcoded in each protocol and will not change often.  And because
 * they are hardcoded, they do not need to be saved to any XML file.
 */
typedef struct _PurpleStatusType       PurpleStatusType;

#define PURPLE_TYPE_STATUS_ATTRIBUTE   (purple_status_attribute_get_type())

typedef struct _PurpleStatusAttribute  PurpleStatusAttribute;

#define PURPLE_TYPE_MOOD               (purple_mood_get_type())

typedef struct _PurpleMood {
	const char *mood;
	const char *description;
	gpointer *padding;
} PurpleMood;

/**
 * A primitive defining the basic structure of a status type.
 */
/*
 * If you add a value to this enum, make sure you update
 * the status_primitive_map and primitive_scores arrays in status.c.
 */
typedef enum
{
	PURPLE_STATUS_UNSET = 0,
	PURPLE_STATUS_OFFLINE,
	PURPLE_STATUS_AVAILABLE,
	PURPLE_STATUS_UNAVAILABLE,
	PURPLE_STATUS_INVISIBLE,
	PURPLE_STATUS_AWAY,
	PURPLE_STATUS_EXTENDED_AWAY,
	PURPLE_STATUS_MOBILE,
	PURPLE_STATUS_TUNE,
	PURPLE_STATUS_MOOD,
	PURPLE_STATUS_NUM_PRIMITIVES
} PurpleStatusPrimitive;

#include "presence.h"

#define PURPLE_TUNE_ARTIST	"tune_artist"
#define PURPLE_TUNE_TITLE	"tune_title"
#define PURPLE_TUNE_ALBUM	"tune_album"
#define PURPLE_TUNE_GENRE	"tune_genre"
#define PURPLE_TUNE_COMMENT	"tune_comment"
#define PURPLE_TUNE_TRACK	"tune_track"
#define PURPLE_TUNE_TIME	"tune_time"
#define PURPLE_TUNE_YEAR	"tune_year"
#define PURPLE_TUNE_URL		"tune_url"
#define PURPLE_TUNE_FULL	"tune_full"

#define PURPLE_MOOD_NAME	"mood"
#define PURPLE_MOOD_COMMENT	"moodtext"

/**
 * A PurpleStatus can be thought of as an "instance" of a PurpleStatusType.
 * If you're familiar with object-oriented programming languages
 * then this should be immediately clear.  Say, for example, that
 * one of your AIM buddies has set himself as "away."  You have a
 * PurpleBuddy node for this person in your buddy list.  Purple wants
 * to mark this buddy as "away," so it creates a new PurpleStatus.
 * The PurpleStatus has its PurpleStatusType set to the "away" state
 * for the oscar protocol.  The PurpleStatus also contains the buddy's
 * away message.  PurpleStatuses are sometimes saved, depending on
 * the context.  The current PurpleStatuses associated with each of
 * your accounts are saved so that the next time you start Purple,
 * your accounts will be set to their last known statuses.  There
 * is also a list of saved statuses that are written to the
 * status.xml file.  Also, each PurpleStatus has a "saveable" boolean.
 * If "saveable" is set to FALSE then the status is NEVER saved.
 * All PurpleStatuses should be inside a PurplePresence.
 *
 * A PurpleStatus is either "independent" or "exclusive."
 * Independent statuses can be active or inactive and they don't
 * affect anything else.  However, you can only have one exclusive
 * status per PurplePresence.  If you activate one exclusive status,
 * then the previous exclusive status is automatically deactivated.
 *
 * A PurplePresence is like a collection of PurpleStatuses (plus some
 * other random info).
 *
 * @see presence.h
 */
struct _PurpleStatus
{
	GObject gparent;
};

/** Base class for all #PurpleStatus's */
struct _PurpleStatusClass {
	GObjectClass parent_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name PurpleStatusPrimitive API                                       */
/**************************************************************************/
/*@{*/

/**
 * Lookup the id of a primitive status type based on the type.  This
 * ID is a unique plain-text name of the status, without spaces.
 *
 * @param type A primitive status type.
 *
 * @return The unique ID for this type.
 */
const char *purple_primitive_get_id_from_type(PurpleStatusPrimitive type);

/**
 * Lookup the name of a primitive status type based on the type.  This
 * name is the plain-English name of the status type.  It is usually one
 * or two words.
 *
 * @param type A primitive status type.
 *
 * @return The name of this type, suitable for users to see.
 */
const char *purple_primitive_get_name_from_type(PurpleStatusPrimitive type);

/**
 * Lookup the value of a primitive status type based on the id.  The
 * ID is a unique plain-text name of the status, without spaces.
 *
 * @param id The unique ID of a primitive status type.
 *
 * @return The PurpleStatusPrimitive value.
 */
PurpleStatusPrimitive purple_primitive_get_type_from_id(const char *id);

/*@}*/

/**************************************************************************/
/** @name PurpleStatusType API                                            */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the PurpleStatusType boxed structure.
 */
GType purple_status_type_get_type(void);

/**
 * Creates a new status type.
 *
 * @param primitive     The primitive status type.
 * @param id            The ID of the status type, or @c NULL to use the id of
 *                      the primitive status type.
 * @param name          The name presented to the user, or @c NULL to use the
 *                      name of the primitive status type.
 * @param saveable      TRUE if the information set for this status by the
 *                      user can be saved for future sessions.
 * @param user_settable TRUE if this is a status the user can manually set.
 * @param independent   TRUE if this is an independent (non-exclusive)
 *                      status type.
 *
 * @return A new status type.
 */
PurpleStatusType *purple_status_type_new_full(PurpleStatusPrimitive primitive,
										  const char *id, const char *name,
										  gboolean saveable,
										  gboolean user_settable,
										  gboolean independent);

/**
 * Creates a new status type with some default values (
 * saveable and not independent).
 *
 * @param primitive     The primitive status type.
 * @param id            The ID of the status type, or @c NULL to use the id of
 *                      the primitive status type.
 * @param name          The name presented to the user, or @c NULL to use the
 *                      name of the primitive status type.
 * @param user_settable TRUE if this is a status the user can manually set.
 *
 * @return A new status type.
 */
PurpleStatusType *purple_status_type_new(PurpleStatusPrimitive primitive,
									 const char *id, const char *name,
									 gboolean user_settable);

/**
 * Creates a new status type with attributes.
 *
 * @param primitive     The primitive status type.
 * @param id            The ID of the status type, or @c NULL to use the id of
 *                      the primitive status type.
 * @param name          The name presented to the user, or @c NULL to use the
 *                      name of the primitive status type.
 * @param saveable      TRUE if the information set for this status by the
 *                      user can be saved for future sessions.
 * @param user_settable TRUE if this is a status the user can manually set.
 * @param independent   TRUE if this is an independent (non-exclusive)
 *                      status type.
 * @param attr_id       The ID of the first attribute.
 * @param attr_name     The name of the first attribute.
 * @param attr_value    The value type of the first attribute.
 * @param ...           Additional attribute information.
 *
 * @return A new status type.
 */
PurpleStatusType *purple_status_type_new_with_attrs(PurpleStatusPrimitive primitive,
												const char *id,
												const char *name,
												gboolean saveable,
												gboolean user_settable,
												gboolean independent,
												const char *attr_id,
												const char *attr_name,
												GValue *attr_value, ...) G_GNUC_NULL_TERMINATED;

/**
 * Destroys a status type.
 *
 * @param status_type The status type to destroy.
 */
void purple_status_type_destroy(PurpleStatusType *status_type);

/**
 * Returns the primitive type of a status type.
 *
 * @param status_type The status type.
 *
 * @return The primitive type of the status type.
 */
PurpleStatusPrimitive purple_status_type_get_primitive(
	const PurpleStatusType *status_type);

/**
 * Returns the ID of a status type.
 *
 * @param status_type The status type.
 *
 * @return The ID of the status type.
 */
const char *purple_status_type_get_id(const PurpleStatusType *status_type);

/**
 * Returns the name of a status type.
 *
 * @param status_type The status type.
 *
 * @return The name of the status type.
 */
const char *purple_status_type_get_name(const PurpleStatusType *status_type);

/**
 * Returns whether or not the status type is saveable.
 *
 * @param status_type The status type.
 *
 * @return TRUE if user-defined statuses based off this type are saveable.
 *         FALSE otherwise.
 */
gboolean purple_status_type_is_saveable(const PurpleStatusType *status_type);

/**
 * Returns whether or not the status type can be set or modified by the
 * user.
 *
 * @param status_type The status type.
 *
 * @return TRUE if the status type can be set or modified by the user.
 *         FALSE if it's a protocol-set setting.
 */
gboolean purple_status_type_is_user_settable(const PurpleStatusType *status_type);

/**
 * Returns whether or not the status type is independent.
 *
 * Independent status types are non-exclusive. If other status types on
 * the same hierarchy level are set, this one will not be affected.
 *
 * @param status_type The status type.
 *
 * @return TRUE if the status type is independent, or FALSE otherwise.
 */
gboolean purple_status_type_is_independent(const PurpleStatusType *status_type);

/**
 * Returns whether the status type is exclusive.
 *
 * @param status_type The status type.
 *
 * @return TRUE if the status type is exclusive, FALSE otherwise.
 */
gboolean purple_status_type_is_exclusive(const PurpleStatusType *status_type);

/**
 * Returns whether or not a status type is available.
 *
 * Available status types are online and possibly invisible, but not away.
 *
 * @param status_type The status type.
 *
 * @return TRUE if the status is available, or FALSE otherwise.
 */
gboolean purple_status_type_is_available(const PurpleStatusType *status_type);

/**
 * Returns the attribute with the specified ID.
 *
 * @param status_type The status type containing the attribute.
 * @param id          The ID of the desired attribute.
 *
 * @return The attribute, if found. NULL otherwise.
 */
PurpleStatusAttribute *purple_status_type_get_attr(const PurpleStatusType *status_type,
										  const char *id);

/**
 * Returns a list of all attributes in a status type.
 *
 * @param status_type The status type.
 *
 * @constreturn The list of attributes.
 */
GList *purple_status_type_get_attrs(const PurpleStatusType *status_type);

/**
 * Find the PurpleStatusType with the given id.
 *
 * @param status_types A list of status types.  Often account->status_types.
 * @param id The unique ID of the status type you wish to find.
 *
 * @return The status type with the given ID, or NULL if one could
 *         not be found.
 */
const PurpleStatusType *purple_status_type_find_with_id(GList *status_types,
													const char *id);

/*@}*/

/**************************************************************************/
/** @name PurpleStatusAttribute API                                       */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the PurpleStatusAttribute boxed structure.
 */
GType purple_status_attribute_get_type(void);

/**
 * Creates a new status attribute.
 *
 * @param id         The ID of the attribute.
 * @param name       The name presented to the user.
 * @param value_type The type of data contained in the attribute.
 *
 * @return A new status attribute.
 */
PurpleStatusAttribute *purple_status_attribute_new(const char *id, const char *name,
									 GValue *value_type);

/**
 * Destroys a status attribute.
 *
 * @param attr The status attribute to destroy.
 */
void purple_status_attribute_destroy(PurpleStatusAttribute *attr);

/**
 * Returns the ID of a status attribute.
 *
 * @param attr The status attribute.
 *
 * @return The status attribute's ID.
 */
const char *purple_status_attribute_get_id(const PurpleStatusAttribute *attr);

/**
 * Returns the name of a status attribute.
 *
 * @param attr The status attribute.
 *
 * @return The status attribute's name.
 */
const char *purple_status_attribute_get_name(const PurpleStatusAttribute *attr);

/**
 * Returns the value of a status attribute.
 *
 * @param attr The status attribute.
 *
 * @return The status attribute's value.
 */
GValue *purple_status_attribute_get_value(const PurpleStatusAttribute *attr);

/*@}*/

/**************************************************************************/
/** @name PurpleMood API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the PurpleMood boxed structure.
 */
GType purple_mood_get_type(void);

/*@}*/

/**************************************************************************/
/** @name PurpleStatus API                                                */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the Status object.
 */
GType purple_status_get_type(void);

/**
 * Creates a new status.
 *
 * @param status_type The type of status.
 * @param presence    The parent presence.
 *
 * @return The new status.
 */
PurpleStatus *purple_status_new(PurpleStatusType *status_type,
							PurplePresence *presence);

/**
 * Sets whether or not a status is active.
 *
 * This should only be called by the account, conversation, and buddy APIs.
 *
 * @param status The status.
 * @param active The active state.
 */
void purple_status_set_active(PurpleStatus *status, gboolean active);

/**
 * Sets whether or not a status is active.
 *
 * This should only be called by the account, conversation, and buddy APIs.
 *
 * @param status The status.
 * @param active The active state.
 * @param args   A list of attributes to set on the status.  This list is
 *               composed of key/value pairs, where each key is a valid
 *               attribute name for this PurpleStatusType.  The list should
 *               be NULL terminated.
 */
void purple_status_set_active_with_attrs(PurpleStatus *status, gboolean active,
									   va_list args);

/**
 * Sets whether or not a status is active.
 *
 * This should only be called by the account, conversation, and buddy APIs.
 *
 * @param status The status.
 * @param active The active state.
 * @param attrs  A list of attributes to set on the status.  This list is
 *               composed of key/value pairs, where each key is a valid
 *               attribute name for this PurpleStatusType.  The list is
 *               not modified or freed by this function.
 */
void purple_status_set_active_with_attrs_list(PurpleStatus *status, gboolean active,
											GList *attrs);

/**
 * Returns the status's type.
 *
 * @param status The status.
 *
 * @return The status's type.
 */
PurpleStatusType *purple_status_get_status_type(const PurpleStatus *status);

/**
 * Returns the status's presence.
 *
 * @param status The status.
 *
 * @return The status's presence.
 */
PurplePresence *purple_status_get_presence(const PurpleStatus *status);

/**
 * Returns the status's type ID.
 *
 * This is a convenience method for
 * purple_status_type_get_id(purple_status_get_type(status)).
 *
 * @param status The status.
 *
 * @return The status's ID.
 */
const char *purple_status_get_id(const PurpleStatus *status);

/**
 * Returns the status's name.
 *
 * This is a convenience method for
 * purple_status_type_get_name(purple_status_get_type(status)).
 *
 * @param status The status.
 *
 * @return The status's name.
 */
const char *purple_status_get_name(const PurpleStatus *status);

/**
 * Returns whether or not a status is independent.
 *
 * This is a convenience method for
 * purple_status_type_is_independent(purple_status_get_type(status)).
 *
 * @param status The status.
 *
 * @return TRUE if the status is independent, or FALSE otherwise.
 */
gboolean purple_status_is_independent(const PurpleStatus *status);

/**
 * Returns whether or not a status is exclusive.
 *
 * This is a convenience method for
 * purple_status_type_is_exclusive(purple_status_get_type(status)).
 *
 * @param status The status.
 *
 * @return TRUE if the status is exclusive, FALSE otherwise.
 */
gboolean purple_status_is_exclusive(const PurpleStatus *status);

/**
 * Returns whether or not a status is available.
 *
 * Available statuses are online and possibly invisible, but not away or idle.
 *
 * This is a convenience method for
 * purple_status_type_is_available(purple_status_get_type(status)).
 *
 * @param status The status.
 *
 * @return TRUE if the status is available, or FALSE otherwise.
 */
gboolean purple_status_is_available(const PurpleStatus *status);

/**
 * Returns the active state of a status.
 *
 * @param status The status.
 *
 * @return The active state of the status.
 */
gboolean purple_status_is_active(const PurpleStatus *status);

/**
 * Returns whether or not a status is considered 'online'
 *
 * @param status The status.
 *
 * @return TRUE if the status is considered online, FALSE otherwise
 */
gboolean purple_status_is_online(const PurpleStatus *status);

/**
 * Returns the value of an attribute in a status with the specified ID.
 *
 * @param status The status.
 * @param id     The attribute ID.
 *
 * @return The value of the attribute.
 */
GValue *purple_status_get_attr_value(const PurpleStatus *status,
									  const char *id);

/**
 * Returns the boolean value of an attribute in a status with the specified ID.
 *
 * @param status The status.
 * @param id     The attribute ID.
 *
 * @return The boolean value of the attribute.
 */
gboolean purple_status_get_attr_boolean(const PurpleStatus *status,
									  const char *id);

/**
 * Returns the integer value of an attribute in a status with the specified ID.
 *
 * @param status The status.
 * @param id     The attribute ID.
 *
 * @return The integer value of the attribute.
 */
int purple_status_get_attr_int(const PurpleStatus *status, const char *id);

/**
 * Returns the string value of an attribute in a status with the specified ID.
 *
 * @param status The status.
 * @param id     The attribute ID.
 *
 * @return The string value of the attribute.
 */
const char *purple_status_get_attr_string(const PurpleStatus *status,
										const char *id);

/**
 * Compares two statuses for availability.
 *
 * @param status1 The first status.
 * @param status2 The second status.
 *
 * @return -1 if @a status1 is more available than @a status2.
 *          0 if @a status1 is equal to @a status2.
 *          1 if @a status2 is more available than @a status1.
 */
gint purple_status_compare(const PurpleStatus *status1, const PurpleStatus *status2);

/*@}*/

/**************************************************************************/
/** @name Statuses subsystem                                                */
/**************************************************************************/
/*@{*/

/**
 * Get the handle for the status subsystem.
 *
 * @return the handle to the status subsystem
 */
void *purple_statuses_get_handle(void);

/**
 * Initializes the status subsystem.
 */
void purple_statuses_init(void);

/**
 * Uninitializes the status subsystem.
 */
void purple_statuses_uninit(void);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_STATUS_H_ */

