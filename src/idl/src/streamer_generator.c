/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>

#include "idl/streamer_generator.h"
#include "idl/string_utils.h"
#include "idl/processor.h"

static const char* struct_write_func_fmt = "size_t write_struct(const %s &write, void *data, size_t position)";
static const char* primitive_calc_alignment_fmt = "(%d - position%%%d)%%%d;";
static const char* primitive_calc_alignment_even_odd = "position%2;";
static const char* primitive_incr_alignment_fmt = "  position += alignmentbytes;";
static const char* primitive_write_func_padding_fmt = "  memset(data+position,0x0,%d);  //setting padding bytes to 0x0\n";
static const char* primitive_write_func_alignment_fmt = "  memset(data+position,0x0,alignmentbytes);  //setting alignment bytes to 0x0\n";
static const char* primitive_write_func_write_fmt = "  memcpy(data+position,&write.%s(),%d);";
static const char* incr_comment = "  //moving position indicator\n";
static const char* instance_write_func_fmt = "  position = write_struct(write.%s(), data, position);\n";
static const char* namespace_declaration_fmt = "namespace %s\n";
static const char* struct_write_size_func_fmt = "size_t write_size(const %s &write, size_t offset)";
static const char* primitive_incr_pos = "  position += %d;";
static const char* instance_size_func_calc_fmt = "  position += write_size(write.%s(), position);\n";

static const char* struct_read_func_fmt = "size_t read_struct(%s &read, void *data, size_t position)";
static const char* struct_read_size_func_fmt = "size_t %s_read_size(void *data, size_t offset)";
static const char* struct_read_size_func_call_fmt = "  position += %s_read_size(data, position);\n";
static const char* primitive_read_func_read_fmt = "  memcpy(&read.%s() data+position,%d);";
static const char* instance_read_func_fmt = "  position = read_struct(write.%s(), data, position);\n";


struct context
{
  streamer_t* str;
  char* context;
  ostream_t* header_stream;
  ostream_t* write_size_stream;
  ostream_t* write_stream;
  ostream_t* read_size_stream;
  ostream_t* read_stream;
  size_t depth;
  int currentalignment;
  int accumulatedalignment;
  int alignmentpresent;
};

struct ostream {
  char* buffer;
  size_t bufsize;
  size_t* indentlength;
};

#ifndef _WIN32
#define strcpy_s(ptr, len, str) strcpy(ptr, str)
#define sprintf_s(ptr, len, str, ...) sprintf(ptr, str, __VA_ARGS__)
#define strcat_s(ptr, len, str) strcat(ptr, str)
#endif


static char* generatealignment(char* oldstring, int alignto)
{
  char* returnval;
  if (alignto < 2)
  {
    size_t len = strlen("0" + 1);
    returnval = realloc(oldstring, len);
    strcpy_s(returnval, len, "0");
  }
  else if (alignto == 2)
  {
    size_t len = strlen(primitive_calc_alignment_even_odd) + 1;
    returnval = realloc(oldstring, len);
    strcpy_s(returnval, len, primitive_calc_alignment_even_odd);
  }
  else
  {
    size_t len = strlen(primitive_calc_alignment_fmt) - 10 + 5 + 1;
    returnval = realloc(oldstring, len);
    sprintf_s(returnval, len, primitive_calc_alignment_fmt, alignto, alignto, alignto);
  }
  return returnval;
}

streamer_t* create_streamer(const char* filename_prefix)
{
  size_t preflen = strlen(filename_prefix);
  size_t headlen = preflen + +strlen(".h") + 1;
  size_t impllen = preflen + +strlen(".cpp") + 1;
  char* header = malloc(headlen);
  char* impl = malloc(impllen);
  header[0] = 0x0;
  impl[0] = 0x0;
  strcat_s(header, headlen, filename_prefix);
  strcat_s(header, headlen, ".h");
  strcat_s(impl, impllen, filename_prefix);
  strcat_s(impl, impllen, ".cpp");


  //generate file names

  FILE* hfile = fopen(header, "w");
  FILE* ifile = fopen(impl, "w");
  free(header);
  free(impl);
  if (NULL == hfile || NULL == ifile)
  {
    if (hfile)
      fclose(hfile);
    if (ifile)
      fclose(ifile);
    return NULL;
  }

  printf("files opened\n");

  streamer_t* ptr = malloc(sizeof(streamer_t));
  if (NULL != ptr)
  {
    ptr->header_file = hfile;
    ptr->impl_file = ifile;
  }
  return ptr;
}

void destruct_streamer(streamer_t* str)
{
  if (NULL == str)
    return;
  if (str->header_file != NULL)
    fclose (str->header_file);
  if (str->impl_file != NULL)
    fclose (str->impl_file);
  free (str);
}

static ostream_t* create_ostream(size_t *indent)
{
  ostream_t* returnptr = malloc(sizeof(*returnptr));
  returnptr->buffer = NULL;
  returnptr->bufsize = 0;
  returnptr->indentlength = indent;
  return returnptr;
}

static void destruct_ostream(ostream_t* str)
{
  if (str->buffer)
    free(str->buffer);
  free(str);
}

static void clear_ostream(ostream_t* str)
{
  if (str->buffer)
    free(str->buffer);
  str->buffer = NULL;
  str->bufsize = 0;
}

static void flush_ostream(ostream_t* str, FILE* f)
{
  if (str->buffer != NULL)
  {
    if (f == NULL)
      printf("%s", str->buffer);
    else
      fprintf(f, "%s", str->buffer);
    clear_ostream(str);
  }
}

static void append_ostream(ostream_t* str, const char* toappend, bool indent)
{
  size_t appendlength = strlen(toappend);
  size_t indentlength = indent ? 2 * *(str->indentlength) : 0;
  if (str->buffer == NULL)
  {
    str->bufsize = appendlength + 1 + indentlength;
    str->buffer = malloc(str->bufsize);
    memset(str->buffer, ' ', indentlength);
    memcpy(str->buffer + indentlength, toappend, appendlength + 1);
  }
  else
  {
    size_t newlength = appendlength + str->bufsize + indentlength;
    char* newbuffer = malloc(newlength);
    memcpy(newbuffer, str->buffer, str->bufsize);
    memset(newbuffer + str->bufsize - 1, ' ', indentlength);
    memcpy(newbuffer + str->bufsize - 1 + indentlength, toappend, appendlength + 1);
    free(str->buffer);
    str->buffer = newbuffer;
    str->bufsize = newlength;
  }
}

context_t* create_context(streamer_t* str, const char* ctx)
{
  context_t* ptr = malloc(sizeof(context_t));
  if (NULL != ptr)
  {
    ptr->str = str;
    ptr->depth = 0;
    size_t len = strlen(ctx) + 1;
    ptr->context = malloc(len);
    ptr->context[strlen(ctx)] = 0x0;
    strcpy_s(ptr->context, len, ctx);
    ptr->currentalignment = -1;
    ptr->accumulatedalignment = 0;
    ptr->alignmentpresent = 0;
    ptr->header_stream = create_ostream(&ptr->depth);
    ptr->write_size_stream = create_ostream(&ptr->depth);
    ptr->write_stream = create_ostream(&ptr->depth);
    ptr->read_size_stream = create_ostream(&ptr->depth);
    ptr->read_stream = create_ostream(&ptr->depth);
  }
  printf("new context generated: %s\n", ctx);
  return ptr;
}

static void clear_context(context_t* ctx)
{
  clear_ostream(ctx->header_stream);
  clear_ostream(ctx->write_stream);
  clear_ostream(ctx->write_size_stream);
  clear_ostream(ctx->read_stream);
  clear_ostream(ctx->read_size_stream);
}

static void flush_context(context_t* ctx)
{
  flush_ostream(ctx->header_stream, ctx->str->header_file);
  flush_ostream(ctx->write_stream, ctx->str->impl_file);
  flush_ostream(ctx->write_size_stream, ctx->str->impl_file);
  flush_ostream(ctx->read_stream, ctx->str->impl_file);
  flush_ostream(ctx->read_size_stream, ctx->str->impl_file);
}

void close_context(context_t* ctx)
{
  //add closing statements to buffers?

  flush_context(ctx);
  destruct_ostream(ctx->header_stream);
  destruct_ostream(ctx->write_size_stream);
  destruct_ostream(ctx->write_stream);
  destruct_ostream(ctx->read_size_stream);
  destruct_ostream(ctx->read_stream);
  printf("context closed: %s\n", ctx->context);
  free(ctx->context);
}

idl_retcode_t print_node(size_t depth, idl_node_t* node)
{
  if (NULL == node) return IDL_RETCODE_OK;
  for (size_t i = 0; i < depth; i++) printf("  ");
  printf("%s:", node->name);

  if (node->flags & IDL_MODULE)
  {
    printf(" MODULE");
    printf(" %s",node->type._case.declarator);
  }
  else if (node->flags & IDL_CONSTR_TYPE)
  {
    printf(" CONSTRUCTED TYPE: ");
    switch (node->flags & 0xf)
    {
    case 0x1:
      printf(" STRUCT");
      break;
    case 0x2:
      printf(" UNION");
      break;
    case 0x3:
      printf(" ENUM");
      break;
    default:
      printf("UNKNOWN");
    }
  }
  else if (node->flags & IDL_TEMPL_TYPE)
  {
    printf(" TEMPLATE TYPE: ");
    switch (node->flags & 0xf)
    {
    case 0x1:
      printf(" SEQUENCE");
      break;
    case 0x2:
      printf(" STRING");
      break;
    case 0x3:
      printf(" WSTRING");
      break;
    case 0x4:
      printf(" FIXEDPT");
      break;
    default:
      printf(" UNKNOWN");
    }

  }
  else if ((node->flags & IDL_INTEGER_TYPE) == IDL_INTEGER_TYPE)
  {
    if (node->flags & IDL_UNSIGNED) printf("UNSIGNED ");
    printf("INT_");
    switch ((node->flags & 0xf) & ~IDL_UNSIGNED)
    {
    case 0x2:
      printf("8");
      break;
    case 0x4:
      printf("16");
      break;
    case 0x6:
      printf("32");
      break;
    case 0x8:
      printf("64");
      break;
    }
    //printf(" 0x%x", node->flags);
    printf(" %s", node->type.member.declarator);
  }
  else if ((node->flags & IDL_FLOATING_PT_TYPE) == IDL_FLOATING_PT_TYPE)
  {
    if (node->flags & IDL_UNSIGNED) printf("UNSIGNED ");
    printf("FLOAT_");
    switch (node->flags & 0xf)
    {
    case 0x2:
      printf("32");
      break;
    case 0x4:
    case 0x6:
      printf("64");
      break;
    }
    printf(" %s", node->type.member.declarator);
  }
  else if (node->flags & IDL_BASE_TYPE)
  {
    switch (node->flags & 0xf)
    {
    case 0x1:
      printf("CHAR");
      break;
    case 0x2:
      printf("WCHAR");
      break;
    case 0x3:
      printf("BOOL");
      break;
    case 0x4:
      printf("OCTET");
      break;
    default:
      printf(" UNKNOWN");
    }
    printf(" %s", node->type.member.declarator);
  }
  printf("\n");


  if (node->children)
    print_node(depth + 1, node->children);
  if (node->next)
    print_node(depth, node->next);

  return IDL_RETCODE_OK;
}

idl_retcode_t process_node(context_t* ctx, idl_node_t* node)
{
  if (node->flags & IDL_MEMBER)
    process_member(ctx, node);
  else if (node->flags & IDL_MODULE)
    process_module(ctx, node);
  else if (node->flags & IDL_CONSTR_TYPE)
    process_constructed(ctx, node);

  if (node->next)
    process_node(ctx,node->next);

  return IDL_RETCODE_PRECONDITION_NOT_MET;
}

static idl_retcode_t process_instance(context_t* ctx, idl_node_t* node);
static idl_retcode_t process_base(context_t* ctx, idl_node_t* node);
static idl_retcode_t process_template(context_t* ctx, idl_node_t* node);

idl_retcode_t process_member(context_t* ctx, idl_node_t* node)
{
  //printf("member flags: %x\n", node->flags);
  if (node->flags & IDL_BASE_TYPE)
    process_base(ctx, node);
  else if (node->flags & IDL_SCOPED_NAME)
    process_instance(ctx, node);
  else if (node->flags & IDL_TEMPL_TYPE)
    process_template(ctx, node);

  return IDL_RETCODE_OK;
}

idl_retcode_t process_instance(context_t* ctx, idl_node_t* node)
{
  char* cpp11name = get_cpp11_name(node->type.member.declarator);
  size_t bufsize = strlen(instance_write_func_fmt) - 2 + strlen(cpp11name) + 1;
  char* buffer = malloc(bufsize);
  sprintf_s(buffer, bufsize, instance_write_func_fmt, cpp11name);
  append_ostream(ctx->write_stream, buffer,true);

  bufsize = strlen(instance_read_func_fmt) - 2 + strlen(cpp11name) + 1;
  buffer = realloc(buffer, bufsize);
  sprintf_s(buffer, bufsize, instance_read_func_fmt, cpp11name);
  append_ostream(ctx->read_stream, buffer, true);

  bufsize = strlen(struct_read_size_func_call_fmt) - 2 + strlen(cpp11name) + 1;
  buffer = realloc(buffer, bufsize);
  sprintf_s(buffer, bufsize, struct_read_size_func_call_fmt, cpp11name);
  append_ostream(ctx->read_size_stream, buffer, true);

  bufsize = strlen(instance_size_func_calc_fmt) - 2 + strlen(cpp11name) + 1;
  buffer = realloc(buffer, bufsize);
  sprintf_s(buffer, bufsize, instance_size_func_calc_fmt, cpp11name);
  append_ostream(ctx->write_size_stream, buffer, true);

  ctx->accumulatedalignment = 0;
  ctx->currentalignment = -1;

  free(buffer);
  free(cpp11name);
  return IDL_RETCODE_OK;
}

idl_retcode_t process_template(context_t* ctx, idl_node_t* node)
{
  printf("processing template typed member named: %s::%s\n", ctx->context, node->type.member.declarator);
  return IDL_RETCODE_OK;
}

idl_retcode_t process_module(context_t* ctx, idl_node_t* node)
{
  if (node->children)
  {
    char* cpp11name = get_cpp11_name(node->name);
    size_t bufsize = strlen(namespace_declaration_fmt) + strlen(cpp11name) - 2 + 1;
    char* buffer = malloc(bufsize);
    sprintf_s(buffer, bufsize, namespace_declaration_fmt, cpp11name);
    context_t* newctx = create_context(ctx->str, cpp11name);

    newctx->depth = ctx->depth;
    append_ostream(newctx->header_stream, buffer, true);
    append_ostream(newctx->header_stream, "{\n\n", true);
    append_ostream(newctx->write_stream, buffer, true);
    append_ostream(newctx->write_stream, "{\n\n", true);
    newctx->depth++;
    free(cpp11name);
    free(buffer);

    process_node(newctx, node->children);
    newctx->depth--;
    append_ostream(newctx->header_stream, "}\n\n", true);
    append_ostream(newctx->read_size_stream, "}\n\n", true);
    close_context(newctx);
  }

  return IDL_RETCODE_OK;
}

idl_retcode_t process_constructed(context_t* ctx, idl_node_t* node)
{
  if (node->children)
  {
    char* cpp11name = get_cpp11_name(node->name);
    size_t bufsize = strlen(cpp11name) + strlen(struct_write_func_fmt) - 2 + 1;
    char* buffer = malloc(bufsize);
    sprintf_s(buffer, bufsize, struct_write_func_fmt, cpp11name);
    append_ostream(ctx->header_stream, buffer, true);
    append_ostream(ctx->header_stream, ";\n\n",false);
    append_ostream(ctx->write_stream, buffer, true);
    append_ostream(ctx->write_stream, "\n", false);
    append_ostream(ctx->write_stream, "{\n", true);
    
    bufsize = strlen(cpp11name) + strlen(struct_write_size_func_fmt) - 2 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, struct_write_size_func_fmt, cpp11name);
    append_ostream(ctx->header_stream, buffer, true);
    append_ostream(ctx->header_stream, ";\n\n", false);
    append_ostream(ctx->write_size_stream, buffer, true);
    append_ostream(ctx->write_size_stream, "\n", false);
    append_ostream(ctx->write_size_stream, "{\n", true);
    append_ostream(ctx->write_size_stream, "  size_t position = offset;\n",true);

    bufsize = strlen(cpp11name) + strlen(struct_read_func_fmt) - 2 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, struct_read_func_fmt, cpp11name);
    append_ostream(ctx->header_stream, buffer, true);
    append_ostream(ctx->header_stream, ";\n\n", false);
    append_ostream(ctx->read_stream, buffer, true);
    append_ostream(ctx->read_stream, "\n", false);
    append_ostream(ctx->read_stream, "{\n", true);

    bufsize = strlen(cpp11name) + strlen(struct_read_size_func_fmt) - 2 + 1;
    buffer = realloc(buffer, bufsize);
    sprintf_s(buffer, bufsize, struct_read_size_func_fmt, cpp11name);
    append_ostream(ctx->header_stream, buffer, true);
    append_ostream(ctx->header_stream, ";\n\n", false);
    append_ostream(ctx->read_size_stream, buffer, true);
    append_ostream(ctx->read_size_stream, "\n", false);
    append_ostream(ctx->read_size_stream, "{\n", true);
    append_ostream(ctx->read_size_stream, "  size_t position = offset;\n", true);

    ctx->currentalignment = -1;
    ctx->alignmentpresent = 0;
    ctx->accumulatedalignment = 0;

    free(buffer);
    process_node(ctx, node->children);

    append_ostream(ctx->write_size_stream, "  return position-offset;\n", true);
    append_ostream(ctx->write_size_stream, "}\n\n", true);
    append_ostream(ctx->write_stream, "  return position;\n", true);
    append_ostream(ctx->write_stream, "}\n\n", true);
    append_ostream(ctx->read_stream, "  return position;\n", true);
    append_ostream(ctx->read_stream, "}\n\n", true);
    append_ostream(ctx->read_size_stream, "  return position-offset;\n", true);
    append_ostream(ctx->read_size_stream, "}\n\n", true);
    flush_context(ctx);
    free(cpp11name);
  }
  return IDL_RETCODE_OK;
}

idl_retcode_t process_base(context_t* ctx, idl_node_t* node)
{
  char *cpp11name = get_cpp11_name(node->type.member.declarator);
  int bytewidth = 1;
  if ((node->flags & IDL_INTEGER_TYPE) == IDL_INTEGER_TYPE)
  {
    bytewidth = 0x1 << (((node->flags & 0xf) >> 1) - 1);
    //printf("%s integer type of byte width %d\n", cpp11name, bytewidth);
  }
  else if ((node->flags & IDL_FLOATING_PT_TYPE) == IDL_FLOATING_PT_TYPE)
  {
    switch (node->flags & 0xf)
    {
    case 0x2:
      bytewidth = 4;
      break;
    case 0x4:
    case 0x6:
      bytewidth = 8;
      break;
    }
    //printf("%s float type of byte width %d\n", cpp11name, bytewidth);
  }
  size_t bufsize = 0;
  char* buffer = NULL;

  //printf("current alignment: %d, byte width: %d, acc: %d\n", ctx->currentalignment, bytewidth, ctx->accumulatedalignment);
  if (ctx->currentalignment != bytewidth)
  {
    if (0 > ctx->currentalignment && bytewidth != 1)
    {
      if (0 == ctx->alignmentpresent)
      {
        append_ostream(ctx->write_stream, "  size_t alignmentbytes = ", true);
        append_ostream(ctx->read_stream, "  size_t alignmentbytes = ", true);
        ctx->alignmentpresent = 1;
      }
      else
      {
        append_ostream(ctx->write_stream, "  alignmentbytes = ", true);
        append_ostream(ctx->read_stream, "  alignmentbytes = ", true);
      }

      buffer = generatealignment(buffer, bytewidth);
      append_ostream(ctx->write_stream, buffer, false);
      append_ostream(ctx->write_stream, "  //alignment for: ", false);
      append_ostream(ctx->write_stream, cpp11name, false);
      append_ostream(ctx->write_stream, "\n", false);
      append_ostream(ctx->write_stream, primitive_write_func_alignment_fmt, true);
      append_ostream(ctx->write_stream, primitive_incr_alignment_fmt, true);
      append_ostream(ctx->write_stream, incr_comment, false);

      append_ostream(ctx->read_stream, buffer, false);
      append_ostream(ctx->read_stream, "  //alignment for: ", false);
      append_ostream(ctx->read_stream, cpp11name, false);
      append_ostream(ctx->read_stream, "\n", false);
      append_ostream(ctx->read_stream, primitive_incr_alignment_fmt, true);
      append_ostream(ctx->read_stream, incr_comment, false);

      append_ostream(ctx->write_size_stream, "  position += ", true);
      append_ostream(ctx->write_size_stream, buffer, false);
      append_ostream(ctx->write_size_stream, "  //alignment for: ", false);
      append_ostream(ctx->write_size_stream, cpp11name, false);
      append_ostream(ctx->write_size_stream, "\n", false);

      append_ostream(ctx->read_size_stream, "  position += ", true);
      append_ostream(ctx->read_size_stream, buffer, false);
      append_ostream(ctx->read_size_stream, "  //alignment for: ", false);
      append_ostream(ctx->read_size_stream, cpp11name, false);
      append_ostream(ctx->read_size_stream, "\n", false);

      ctx->accumulatedalignment = 0;
      ctx->currentalignment = bytewidth;
    }
    else
    {
      int missingbytes = (bytewidth - (ctx->accumulatedalignment % bytewidth)) % bytewidth;
      if (0 != missingbytes)
      {
        bufsize = strlen(primitive_write_func_padding_fmt) - 2;
        buffer = realloc(buffer, bufsize);
        sprintf_s(buffer, bufsize, primitive_write_func_padding_fmt, missingbytes);
        append_ostream(ctx->write_stream, buffer, true);

        bufsize = strlen(primitive_incr_pos);
        buffer = realloc(buffer, bufsize);
        sprintf_s(buffer, bufsize, primitive_incr_pos, missingbytes);
        append_ostream(ctx->write_size_stream, buffer, true);
        append_ostream(ctx->write_size_stream, "  //padding bytes for: ", false);
        append_ostream(ctx->write_size_stream, cpp11name, false);
        append_ostream(ctx->write_size_stream, "\n", false);

        append_ostream(ctx->read_size_stream, buffer, true);
        append_ostream(ctx->read_size_stream, "  //padding bytes for: ", false);
        append_ostream(ctx->read_size_stream, cpp11name, false);
        append_ostream(ctx->read_size_stream, "\n", false);

        append_ostream(ctx->read_stream, buffer, true);
        append_ostream(ctx->read_stream, "  //padding bytes for: ", false);
        append_ostream(ctx->read_stream, cpp11name, false);
        append_ostream(ctx->read_stream, "\n", false);

        append_ostream(ctx->write_stream, buffer, true);
        append_ostream(ctx->write_stream, incr_comment, false);

        ctx->accumulatedalignment = 0;
      }
    }
  }

  ctx->accumulatedalignment += bytewidth;

  bufsize = strlen(primitive_write_func_write_fmt) - 2 + 1 + strlen(cpp11name) - 2 + 1;
  buffer = realloc(buffer, bufsize);
  sprintf_s(buffer, bufsize, primitive_write_func_write_fmt, cpp11name, bytewidth);
  append_ostream(ctx->write_stream, buffer, true);
  append_ostream(ctx->write_stream, "  //bytes for member: ", false);
  append_ostream(ctx->write_stream, cpp11name, false);
  append_ostream(ctx->write_stream, "\n", false);

  bufsize = strlen(primitive_read_func_read_fmt) - 2 + 1 + strlen(cpp11name) - 2 + 1;
  buffer = realloc(buffer, bufsize);
  sprintf_s(buffer, bufsize, primitive_read_func_read_fmt, cpp11name, bytewidth);
  append_ostream(ctx->read_stream, buffer, true);
  append_ostream(ctx->read_stream, "  //bytes for member: ", false);
  append_ostream(ctx->read_stream, cpp11name, false);
  append_ostream(ctx->read_stream, "\n", false);

  bufsize = strlen(primitive_incr_pos);
  buffer = realloc(buffer, bufsize);
  sprintf_s(buffer, bufsize, primitive_incr_pos, bytewidth);
  append_ostream(ctx->write_size_stream, buffer, true);
  append_ostream(ctx->write_size_stream, "  //bytes for member: ", false);
  append_ostream(ctx->write_size_stream, cpp11name, false);
  append_ostream(ctx->write_size_stream, "\n", false);

  append_ostream(ctx->read_size_stream, buffer, true);
  append_ostream(ctx->read_size_stream, "  //bytes for member: ", false);
  append_ostream(ctx->read_size_stream, cpp11name, false);
  append_ostream(ctx->read_size_stream, "\n", false);

  append_ostream(ctx->write_stream, buffer, true);
  append_ostream(ctx->write_stream, incr_comment, false);

  append_ostream(ctx->read_stream, buffer, true);
  append_ostream(ctx->read_stream, incr_comment, false);

  free(buffer);
  free(cpp11name);
  return IDL_RETCODE_OK;
}

void idl_streamers_generate(char* idl, char* outputname)
{
  idl_tree_t* tree = 0x0;
  idl_parse_string(idl, 0x0, &tree);

  streamer_t *str = create_streamer(outputname);
  context_t* ctx = create_context(str, "");
  process_node(ctx, tree->root);
  close_context(ctx);
  destruct_streamer(str);
}