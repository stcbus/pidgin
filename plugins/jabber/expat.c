/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Jabber
 *  Copyright (C) 1998-1999 The Jabber Team http://jabber.org/
 */

#include <libxode.h>

void expat_startElement(void* userdata, const char* name, const char** atts)
{
    /* get the xmlnode pointed to by the userdata */
    xmlnode *x = userdata;
    xmlnode current = *x;

    if (current == NULL)
    {
        /* allocate a base node */
        current = xmlnode_new_tag(name);
        xmlnode_put_expat_attribs(current, atts);
        *x = current;
    }
    else
    {
        *x = xmlnode_insert_tag(current, name);
        xmlnode_put_expat_attribs(*x, atts);
    }
}

void expat_endElement(void* userdata, const char* name)
{
    xmlnode *x = userdata;
    xmlnode current = *x;

    current->complete = 1;
    current = xmlnode_get_parent(current);

    /* if it's NULL we've hit the top folks, otherwise back up a level */
    if(current != NULL)
        *x = current;
}

void expat_charData(void* userdata, const char* s, int len)
{
    xmlnode *x = userdata;
    xmlnode current = *x;

    xmlnode_insert_cdata(current, s, len);
}


xmlnode xmlnode_str(char *str, int len)
{
    XML_Parser p;
    xmlnode *x, node; /* pointer to an xmlnode */

    if(NULL == str)
        return NULL;

    x = malloc(sizeof(void *));

    *x = NULL; /* pointer to NULL */
    p = XML_ParserCreate(NULL);
    XML_SetUserData(p, x);
    XML_SetElementHandler(p, expat_startElement, expat_endElement);
    XML_SetCharacterDataHandler(p, expat_charData);
    if(!XML_Parse(p, str, len, 1))
    {
        /*        jdebug(ZONE,"xmlnode_str_error: %s",(char *)XML_ErrorString(XML_GetErrorCode(p)));*/
        xmlnode_free(*x);
        *x = NULL;
    }
    node = *x;
    free(x);
    XML_ParserFree(p);
    return node; /* return the xmlnode x points to */
}

xmlnode xmlnode_file(char *file)
{
    XML_Parser p;
    xmlnode *x, node; /* pointer to an xmlnode */
    char buf[BUFSIZ];
    int done, fd, len;

    if(NULL == file)
        return NULL;

    fd = open(file,O_RDONLY);
    if(fd < 0)
        return NULL;

    x = malloc(sizeof(void *));

    *x = NULL; /* pointer to NULL */
    p = XML_ParserCreate(NULL);
    XML_SetUserData(p, x);
    XML_SetElementHandler(p, expat_startElement, expat_endElement);
    XML_SetCharacterDataHandler(p, expat_charData);
    do{
        len = read(fd, buf, BUFSIZ);
        done = len < BUFSIZ;
        if(!XML_Parse(p, buf, len, done))
        {
            /*            jdebug(ZONE,"xmlnode_file_parseerror: %s",(char *)XML_ErrorString(XML_GetErrorCode(p)));*/
            xmlnode_free(*x);
            *x = NULL;
            done = 1;
        }
    }while(!done);

    node = *x;
    XML_ParserFree(p);
    free(x);
    close(fd);
    return node; /* return the xmlnode x points to */
}

int xmlnode2file(char *file, xmlnode node)
{
    char *doc;
    int fd, i;

    if(file == NULL || node == NULL)
        return -1;

    fd = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if(fd < 0)
        return -1;

    doc = xmlnode2str(node);
    i = write(fd,doc,strlen(doc));
    if(i < 0)
        return -1;

    close(fd);
    return 1;
}

void xmlnode_put_expat_attribs(xmlnode owner, const char** atts)
{
    int i = 0;
    if (atts == NULL) return;
    while (atts[i] != '\0')
    {
        xmlnode_put_attrib(owner, atts[i], atts[i+1]);
        i += 2;
    }
}
