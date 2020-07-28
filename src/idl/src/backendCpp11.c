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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "idl/backend.h"
#include "idl/string_utils.h"

typedef struct cpp11_member_state_s
{
  const idl_node_t *node;
  char *member_name;
  char *type_name;
} cpp11_member_state;

typedef struct cpp11_member_context_s
{
  cpp11_member_state *members;
  uint32_t member_count;
} cpp11_member_context;

static char *
get_cpp11_type(const idl_node_t *node);

static char *
get_cpp11_base_type(const idl_node_t *node)
{
  char *cpp11Type = NULL;

  switch (node->flags & IDL_BASE_TYPE_CATEGORY)
  {
  case IDL_INTEGER_TYPE:
    switch(node->flags & IDL_BASE_INTEGER_MASK_IGNORE_SIGN)
    {
    case IDL_INT8:
      cpp11Type = idl_strdup("int8_t");
      break;
    case IDL_INT16:
      cpp11Type = idl_strdup("int16_t");
      break;
    case IDL_INT32:
      cpp11Type = idl_strdup("int32_t");
      break;
    case IDL_INT64:
      cpp11Type = idl_strdup("int64_t");
      break;
    default:
      assert(0);
      break;
    }
    if (node->flags & IDL_UNSIGNED)
    {
      char *signedCpp11Type = cpp11Type;
      size_t unsignedCpp11TypeSize;

      assert(node->flags & (IDL_INT8 | IDL_INT16 | IDL_INT32 | IDL_INT64));
      unsignedCpp11TypeSize = strlen(signedCpp11Type) + 1 + 1;
      cpp11Type = malloc(unsignedCpp11TypeSize);
      snprintf(cpp11Type, unsignedCpp11TypeSize, "u%s", signedCpp11Type);
      free(signedCpp11Type);
    }
    break;
  case IDL_FLOATING_PT_TYPE:
    switch(node->flags & IDL_BASE_FLOAT_MASK)
    {
    case IDL_FLOAT:
      cpp11Type = idl_strdup("float");
      break;
    case IDL_DOUBLE:
    case IDL_LDOUBLE:
      cpp11Type = idl_strdup("double");
      break;
    default:
      assert(0);
      break;
    }
    break;
  default:
    switch(node->flags & IDL_BASE_SIMULTANEOUS_MASK)
    {
    case IDL_CHAR:
      cpp11Type = idl_strdup("char");
      break;
    case IDL_WCHAR:
      cpp11Type = idl_strdup("wchar");
      break;
    case IDL_BOOL:
      cpp11Type = idl_strdup("bool");
      break;
    case IDL_OCTET:
      cpp11Type = idl_strdup("uint8_t");
      break;
    default:
      assert(0);
      break;
    }
    break;
  }
  return cpp11Type;
}

static char *
get_cpp11_templ_type(const idl_node_t *node)
{
  char *cpp11Type = NULL;
  const char *vector_tmplt;
  size_t vector_size;
  char *vector_element;

  switch (node->flags & IDL_TEMPL_TYPE_MASK)
  {
  case IDL_SEQUENCE:
    vector_tmplt = "std::vector<%s>";
    vector_element = get_cpp11_type(node->children);
    vector_size = strlen(vector_tmplt) + strlen(vector_element) - 2 /* Compensate for '%s' */ + 1 /* '\0' */;
    cpp11Type = malloc(vector_size);
    snprintf(cpp11Type, vector_size, vector_tmplt, vector_element);
    free(vector_element);
    break;
  case IDL_STRING:
    cpp11Type = idl_strdup("std::string");
    break;
  case IDL_WSTRING:
    cpp11Type = idl_strdup("std::wstring");
    break;
  case IDL_FIXED_PT:
    assert(0);
    break;
  default:
    assert(0);
    break;
  }

  return cpp11Type;
}

char *
get_cpp11_type(const idl_node_t *node)
{
  char *cpp11Type = NULL;

  switch (node->flags & IDL_CATEGORY_MASK) {
  case IDL_BASE_TYPE:
    cpp11Type = get_cpp11_base_type(node);
    break;
  case IDL_TEMPL_TYPE:
    cpp11Type = get_cpp11_templ_type(node);
    break;
  case IDL_CONSTR_TYPE:
  case IDL_SCOPED_NAME:
    cpp11Type = get_cpp11_name(node->name);
    break;
  default:
    assert(0);
    break;
  }
  return cpp11Type;
}

static idl_retcode_t
enum_default_value(idl_backend_ctx ctx, const idl_node_t *node)
{
  char **def_value = (char **) idl_get_custom_context(ctx);
  *def_value = idl_strdup(node->name);
  return IDL_RETCODE_BREAK_OUT;
}

static char *
get_default_value(idl_backend_ctx ctx, const idl_node_t *node)
{
  char *def_value = NULL;

  switch (node->flags & (IDL_BASE_TYPE | IDL_CONSTR_TYPE))
  {
  case IDL_BASE_TYPE:
    switch (node->flags & IDL_BASE_TYPE_CATEGORY)
    {
    case IDL_INTEGER_TYPE:
      switch(node->flags & IDL_BASE_INTEGER_MASK_IGNORE_SIGN)
      {
      case IDL_INT8:
      case IDL_INT16:
      case IDL_INT32:
      case IDL_INT64:
        def_value = idl_strdup("0");
        break;
      default:
        assert(0);
        break;
      }
      break;
    case IDL_FLOATING_PT_TYPE:
      switch(node->flags & IDL_BASE_FLOAT_MASK)
      {
      case IDL_FLOAT:
        def_value = idl_strdup("0.0f");
        break;
      case IDL_DOUBLE:
      case IDL_LDOUBLE:
        def_value = idl_strdup("0.0");
        break;
      default:
        assert(0);
        break;
      }
      break;
    default:
      switch(node->flags & IDL_BASE_SIMULTANEOUS_MASK)
      {
      case IDL_CHAR:
      case IDL_WCHAR:
      case IDL_OCTET:
        def_value = idl_strdup("0");
        break;
      case IDL_BOOL:
        def_value = idl_strdup("false");
        break;
      default:
        assert(0);
        break;
      }
      break;
    }
    break;
  case IDL_CONSTR_TYPE:
    switch (node->flags & IDL_CONSTR_TYPE_MASK)
    {
    case IDL_ENUM:
    {
      idl_retcode_t result;
      void *custom_context = idl_get_custom_context(ctx);
      idl_reset_custom_context(ctx);
      idl_set_custom_context(ctx, &def_value);
      result = idl_walk_children(ctx, node, enum_default_value, IDL_MASK_ALL);
      assert(result == IDL_RETCODE_BREAK_OUT);
      idl_reset_custom_context(ctx);
      idl_set_custom_context(ctx, &custom_context);
      break;
    }
    default:
      /* Other constructed types determine their default value in their constructor. */
      break;
    }
    break;
  default:
    /* Other types determine their default value in their constructor. */
    break;
  }
  return def_value;
}

static idl_retcode_t
cpp11_scope_walk(idl_backend_ctx ctx, const idl_node_t *node);

static idl_retcode_t
on_module_open(idl_backend_ctx ctx, const idl_node_t *node)
{
  idl_retcode_t result;
  char *cpp11Name = get_cpp11_name(node->name);

  idl_file_out_printf(ctx, "namespace %s {\n", cpp11Name);
  idl_indent_incr(ctx);
  result = idl_walk_children(ctx, node, cpp11_scope_walk, IDL_MODULE | IDL_CONSTR_TYPE);
  idl_indent_decr(ctx);
  idl_file_out_printf(ctx, "};\n");

  free(cpp11Name);

  return result;
}

static idl_retcode_t
count_members(idl_backend_ctx ctx, const idl_node_t *node)
{
  uint32_t *nr_members = (uint32_t *) idl_get_custom_context(ctx);
  ++(*nr_members);
  return IDL_RETCODE_OK;
}

#if 0
static idl_retcode_t
declarator_count_members(idl_backend_ctx ctx, const idl_node_t *node)
{
  return idl_walk_children(ctx, node, count_members, IDL_MASK_ALL);
}
#endif

static idl_retcode_t
get_cpp11_member_data(idl_backend_ctx ctx, const idl_node_t *node)
{
  cpp11_member_context *member_ctx = (cpp11_member_context *) idl_get_custom_context(ctx);
  member_ctx->members[member_ctx->member_count].node = node;
  member_ctx->members[member_ctx->member_count].type_name = get_cpp11_type(node);
  member_ctx->members[member_ctx->member_count].member_name = get_cpp11_name(node->type.member.declarator);
  ++(member_ctx->member_count);
  return IDL_RETCODE_OK;
}

#if 0
static idl_retcode_t
declarator_get_member_data(idl_backend_ctx ctx, const idl_node_t *node)
{
  return idl_walk_children(ctx, node, get_cpp11_member_data, IDL_MASK_ALL);
}
#endif

static void
struct_generate_typedefs(idl_backend_ctx ctx)
{
  cpp11_member_context *member_ctx = (cpp11_member_context *) idl_get_custom_context(ctx);
  bool first_typedef = true;

  for (uint32_t i = 0; i < member_ctx->member_count; ++i)
  {
    uint32_t flags = member_ctx->members[i].node->flags;
    if ((flags & IDL_TEMPL_TYPE) && (flags & IDL_SEQUENCE /* | IDL_ARRAY_TYPE */))
    {
      if (first_typedef)
      {
        idl_file_out_printf(ctx, "public:\n");
        idl_indent_incr(ctx);
        first_typedef = false;
      }
      idl_file_out_printf(ctx, "typedef %s _%s_seq;\n\n",
          member_ctx->members[i].type_name,
          member_ctx->members[i].member_name);
    }
  }
  if (!first_typedef) idl_indent_decr(ctx);
}

static void
struct_generate_attributes(idl_backend_ctx ctx)
{
  cpp11_member_context *member_ctx = (cpp11_member_context *) idl_get_custom_context(ctx);

  idl_file_out_printf(ctx, "private:\n");
  /* Declare all the member attributes. */
  idl_indent_incr(ctx);
  for (uint32_t i = 0; i < member_ctx->member_count; ++i)
  {
    idl_file_out_printf(ctx, "%s %s_;\n",
        member_ctx->members[i].type_name,
        member_ctx->members[i].member_name);
  }
  idl_indent_decr(ctx);
  idl_file_out_printf(ctx, "\n");
}

static void
struct_generate_constructors_and_operators(idl_backend_ctx ctx, const char *struct_name)
{
  cpp11_member_context *member_ctx = (cpp11_member_context *) idl_get_custom_context(ctx);
  bool def_value_present = false;

  /* Start building default (empty) constructor. */
  idl_file_out_printf(ctx, "public:\n");
  idl_indent_incr(ctx);
  idl_file_out_printf(ctx, "%s()", struct_name);

  /* Make double indent for member initialization list */
  idl_indent_double_incr(ctx);
  for (uint32_t i = 0; i < member_ctx->member_count; ++i)
  {
    char *defValue = get_default_value(ctx, member_ctx->members[i].node);
    if (defValue) {
      if (!def_value_present)
      {
        idl_file_out_printf_no_indent(ctx, " :\n");
        def_value_present = true;
      }
      else
      {
        idl_file_out_printf_no_indent(ctx, ",\n");
      }
      idl_file_out_printf(ctx, "%s_(%s)", member_ctx->members[i].member_name, defValue);
      free(defValue);
    }
  }
  idl_file_out_printf_no_indent(ctx, " {}\n\n");
  idl_indent_double_decr(ctx);

  /* Start building constructor that inits all parameters explicitly. */
  idl_file_out_printf(ctx, "explicit %s(\n", struct_name);
  idl_indent_double_incr(ctx);
  for (uint32_t i = 0; i < member_ctx->member_count; ++i)
  {
    idl_file_out_printf(ctx, "%s %s%s",
        member_ctx->members[i].type_name,
        member_ctx->members[i].member_name,
        (i == (member_ctx->member_count - 1) ? ") :\n" : ",\n"));
  }
  idl_indent_double_incr(ctx);
  for (uint32_t i = 0; i < member_ctx->member_count; ++i)
  {
    idl_file_out_printf(ctx, "%s_(%s)%s",
        member_ctx->members[i].member_name,
        member_ctx->members[i].member_name,
        (i == (member_ctx->member_count - 1) ? " {}\n\n" : ",\n"));
  }
  idl_indent_double_decr(ctx);
  idl_indent_double_decr(ctx);
#if 0
  /* Start building the copy constructor. */
  idl_file_out_printf(ctx, "%s(const %s &_other) : \n", struct_name, struct_name);
  idl_indent_double_incr(ctx);
  for (uint32_t i = 0; i < member_ctx->member_count; ++i)
  {
    idl_file_out_printf(ctx, "%s_(_other.%s_)%s",
        member_ctx->members[i].member_name,
        member_ctx->members[i].member_name,
        (i == (member_ctx->member_count - 1) ? " {}\n\n" : ",\n"));
  }
  idl_indent_double_decr(ctx);

  /* Start building the move constructor. */
  idl_file_out_printf(ctx, "%s(%s &&_other) : \n", struct_name, struct_name);
  idl_indent_double_incr(ctx);
  for (uint32_t i = 0; i < member_ctx->member_count; ++i)
  {
    idl_file_out_printf(ctx, "%s_(::std::move(_other.%s_))%s",
        member_ctx->members[i].member_name,
        member_ctx->members[i].member_name,
        (i == (member_ctx->member_count - 1) ? " {}\n\n" : ",\n"));
  }
  idl_indent_double_decr(ctx);

  /* Start building the move assignment operator. */
  idl_file_out_printf(ctx, "%s& operator=(%s &&_other)\n", struct_name, struct_name);
  idl_file_out_printf(ctx, "{\n");
  idl_indent_incr(ctx);
  idl_file_out_printf(ctx, "if (this != &_other)\n");
  idl_file_out_printf(ctx, "{\n");
  idl_indent_incr(ctx);
  for (uint32_t i = 0; i < member_ctx->member_count; ++i)
  {
    idl_file_out_printf(ctx, "%s_ = ::std::move(_other.%s_);\n",
        member_ctx->members[i].member_name,
        member_ctx->members[i].member_name);
  }
  idl_indent_decr(ctx);
  idl_file_out_printf(ctx, "}\n");
  idl_file_out_printf(ctx, "return *this;\n");
  idl_indent_decr(ctx);
  idl_file_out_printf(ctx, "}\n");

  /* Start building the const assignment operator. */
  idl_file_out_printf(ctx, "%s& operator=(const %s &_other)\n", struct_name, struct_name);
  idl_file_out_printf(ctx, "{\n");
  idl_indent_incr(ctx);
  idl_file_out_printf(ctx, "if (this != &_other)\n");
  idl_file_out_printf(ctx, "{\n");
  idl_indent_incr(ctx);
  for (uint32_t i = 0; i < member_ctx->member_count; ++i)
  {
    idl_file_out_printf(ctx, "%s_ = _other.%s_;\n",
        member_ctx->members[i].member_name,
        member_ctx->members[i].member_name);
  }
  idl_indent_decr(ctx);
  idl_file_out_printf(ctx, "}\n");
  idl_file_out_printf(ctx, "return *this;\n");
  idl_indent_decr(ctx);
  idl_file_out_printf(ctx, "}\n\n");

  /* Start building the equals operator. */
  idl_file_out_printf(ctx, "bool operator==(const %s& _other) const\n", struct_name);
  idl_file_out_printf(ctx, "{\n");
  idl_indent_incr(ctx);
  idl_file_out_printf(ctx, "return\n");
  idl_indent_double_incr(ctx);
  for (uint32_t i = 0; i < member_ctx->member_count; ++i)
  {
      idl_file_out_printf(ctx, "%s_ == _other.%s_%s\n",
          member_ctx->members[0].member_name,
          member_ctx->members[0].member_name,
          (i == (member_ctx->member_count - 1) ? ";" : " &&"));
  }
  idl_indent_double_decr(ctx);
  idl_indent_decr(ctx);
  idl_file_out_printf(ctx, "}\n\n");

  /* Start building the not-equals operator. */
  idl_file_out_printf(ctx, "bool operator!=(const %s& _other) const\n", struct_name);
  idl_file_out_printf(ctx, "{\n");
  idl_indent_incr(ctx);
  idl_file_out_printf(ctx, "return !(*this == _other);\n");
  idl_indent_decr(ctx);
  idl_file_out_printf(ctx, "}\n\n");
#endif
  idl_indent_decr(ctx);
}

static void
struct_generate_getters_setters(idl_backend_ctx ctx)
{
  cpp11_member_context *member_ctx = (cpp11_member_context *) idl_get_custom_context(ctx);

  /* Start building the getter/setter methods for each attribute. */
  idl_indent_incr(ctx);
  for (uint32_t i = 0; i < member_ctx->member_count; ++i)
  {
    idl_file_out_printf(ctx, "%s %s() const { return this->%s_; }\n",
        member_ctx->members[i].type_name,
        member_ctx->members[i].member_name,
        member_ctx->members[i].member_name);
    idl_file_out_printf(ctx, "%s& %s() { return this->%s_; }\n",
        member_ctx->members[i].type_name,
        member_ctx->members[i].member_name,
        member_ctx->members[i].member_name);
    idl_file_out_printf(ctx, "void %s(%s _val_) { this->%s_ = _val_; }\n",
        member_ctx->members[i].member_name,
        member_ctx->members[i].type_name,
        member_ctx->members[i].member_name);
    if (idl_is_reference(member_ctx->members[i].node)) {
      idl_file_out_printf(ctx, "void %s(%s&& _val_) { this->%s_ = _val_; }\n",
          member_ctx->members[i].member_name,
          member_ctx->members[i].type_name,
          member_ctx->members[i].member_name);
    }
  }
  idl_indent_decr(ctx);
}

static idl_retcode_t
on_struct_open(idl_backend_ctx ctx, const idl_node_t *node)
{
  idl_retcode_t result;
  uint32_t nr_members = 0;
  char *cpp11Name = get_cpp11_name(node->name);
  cpp11_member_context member_ctx;

  result = idl_set_custom_context(ctx, &nr_members);
  if (result) return result;
  idl_walk_children(ctx, node, count_members, IDL_MASK_ALL);
  idl_reset_custom_context(ctx);

  member_ctx.members = malloc(sizeof(cpp11_member_state) * nr_members);
  member_ctx.member_count = 0;
  result = idl_set_custom_context(ctx, &member_ctx);
  if (result) return result;
  idl_walk_children(ctx, node, get_cpp11_member_data, IDL_MASK_ALL);

  idl_file_out_printf(ctx, "class %s {\n", cpp11Name);

  /* Generate typedefs for all (anonymous) sequence attributes. */
  struct_generate_typedefs(ctx);

  /* Create (private) struct attributes. */
  struct_generate_attributes(ctx);

  /* Create constructors and operators. */
  struct_generate_constructors_and_operators(ctx, cpp11Name);

  /* Create the getters and setters. */
  struct_generate_getters_setters(ctx);

  idl_file_out_printf(ctx, "};\n\n");

  idl_reset_custom_context(ctx);
  for (uint32_t i = 0; i < nr_members; ++i)
  {
    free(member_ctx.members[i].member_name);
    free(member_ctx.members[i].type_name);
  }
  free(member_ctx.members);
  free(cpp11Name);

  return result;
}


static idl_retcode_t
cpp11_scope_walk(idl_backend_ctx ctx, const idl_node_t *node)
{
  idl_retcode_t result = IDL_RETCODE_OK;

  switch (node->flags & IDL_CATEGORY_MASK)
  {
  case IDL_MODULE:
    result = on_module_open(ctx, node);
    break;
  case IDL_CONSTR_TYPE:
    switch(node->flags)
    {
    case IDL_STRUCT:
      result = on_struct_open(ctx, node);
      break;
    }
    break;
  default:
    result = IDL_RETCODE_INVALID_PARSETREE;
    break;
  }

  return result;
}

idl_retcode_t
idl_backendGenerate(idl_backend_ctx ctx, const idl_tree_t *parse_tree)
{
  idl_retcode_t result;

  result = idl_walk_current_scope(ctx, parse_tree->root, cpp11_scope_walk, IDL_MASK_ALL);

  return result;
}



