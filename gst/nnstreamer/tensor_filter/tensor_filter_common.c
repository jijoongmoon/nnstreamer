/**
 * Copyright (C) 2019 Parichay Kapoor <pk.kapoor@samsung.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 */
/**
 * @file	tensor_filter_common.c
 * @date	28 Aug 2019
 * @brief	Common functions for various tensor_filters
 * @see	  http://github.com/nnsuite/nnstreamer
 * @author	Parichay Kapoor <pk.kapoor@samsung.com>
 * @author	MyungJoo Ham <myungjoo.ham@samsung.com>
 * @bug	  No known bugs except for NYI items
 *
 */

#include <string.h>

#include <tensor_common.h>

#include "tensor_filter_common.h"

/**
 * Basic elements to form accelerator regex forming
 */
#define REGEX_ACCL_ELEM_START "("
#define REGEX_ACCL_ELEM_PREFIX "(?<!!)"
#define REGEX_ACCL_ELEM_SUFFIX ""
#define REGEX_ACCL_ELEM_DELIMITER "|"
#define REGEX_ACCL_ELEM_END ")?"

#define REGEX_ACCL_START "(^(true)[:]?([(]?("
#define REGEX_ACCL_PREFIX ""
#define REGEX_ACCL_SUFFIX ""
#define REGEX_ACCL_DELIMITER "|"
#define REGEX_ACCL_END ")*[)]?))"

static const gchar *regex_accl_utils[] = {
  REGEX_ACCL_START,
  REGEX_ACCL_PREFIX,
  REGEX_ACCL_SUFFIX,
  REGEX_ACCL_DELIMITER,
  REGEX_ACCL_END,
  NULL
};

static const gchar *regex_accl_elem_utils[] = {
  REGEX_ACCL_ELEM_START,
  REGEX_ACCL_ELEM_PREFIX,
  REGEX_ACCL_ELEM_SUFFIX,
  REGEX_ACCL_ELEM_DELIMITER,
  REGEX_ACCL_ELEM_END,
  NULL
};

/**
 * @brief Free memory
 */
#define g_free_const(x) g_free((void*)(long)(x))
#define g_strfreev_const(x) g_strfreev((void*)(long)(x))

/**
 * @brief parse user given string to extract list of accelerators based on given regex
 */
static GList *parse_accl_hw_all (const gchar * accelerators,
    const gchar ** supported_accelerators);

/**
 * @brief GstTensorFilter properties.
 */
enum
{
  PROP_0,
  PROP_SILENT,
  PROP_FRAMEWORK,
  PROP_MODEL,
  PROP_INPUT,
  PROP_INPUTTYPE,
  PROP_INPUTNAME,
  PROP_INPUTLAYOUT,
  PROP_OUTPUT,
  PROP_OUTPUTTYPE,
  PROP_OUTPUTNAME,
  PROP_OUTPUTLAYOUT,
  PROP_CUSTOM,
  PROP_SUBPLUGINS,
  PROP_ACCELERATOR,
  PROP_IS_UPDATABLE,
};

/**
 * @brief Initialize the tensors layout.
 */
static void
gst_tensors_layout_init (tensors_layout layout)
{
  int i;

  for (i = 0; i < NNS_TENSOR_SIZE_LIMIT; i++) {
    layout[i] = _NNS_LAYOUT_ANY;
  }
}

/**
 * @brief Get tensor layout from string input.
 * @return Corresponding tensor_layout.
 */
static tensor_layout
gst_tensor_parse_layout_string (const gchar * layoutstr)
{
  gsize len;
  gchar *layout_string;
  tensor_layout layout = _NNS_LAYOUT_ANY;

  if (layoutstr == NULL)
    return layout;

  /* remove spaces */
  layout_string = g_strdup (layoutstr);
  g_strstrip (layout_string);

  len = strlen (layout_string);

  if (len == 0) {
    g_free (layout_string);
    return layout;
  }

  if (g_ascii_strcasecmp (layoutstr, "NCHW") == 0) {
    layout = _NNS_LAYOUT_NCHW;
  } else if (g_ascii_strcasecmp (layoutstr, "NHWC") == 0) {
    layout = _NNS_LAYOUT_NHWC;
  } else if (g_ascii_strcasecmp (layoutstr, "ANY") == 0) {
    layout = _NNS_LAYOUT_ANY;
  } else {
    GST_WARNING ("Invalid layout, defaulting to none layout.");
    layout = _NNS_LAYOUT_NONE;
  }

  g_free (layout_string);
  return layout;
}

/**
 * @brief Parse the string of tensor layouts
 * @param layout layout of the tensors
 * @param layout_string string of layout
 * @return number of parsed layouts
 */
static guint
gst_tensors_parse_layouts_string (tensors_layout layout,
    const gchar * layout_string)
{
  guint num_layouts = 0;

  g_return_val_if_fail (layout != NULL, 0);

  if (layout_string) {
    guint i;
    gchar **str_layouts;

    str_layouts = g_strsplit_set (layout_string, ",.", -1);
    num_layouts = g_strv_length (str_layouts);

    if (num_layouts > NNS_TENSOR_SIZE_LIMIT) {
      GST_WARNING ("Invalid param, layouts (%d) max (%d)\n",
          num_layouts, NNS_TENSOR_SIZE_LIMIT);

      num_layouts = NNS_TENSOR_SIZE_LIMIT;
    }

    for (i = 0; i < num_layouts; i++) {
      layout[i] = gst_tensor_parse_layout_string (str_layouts[i]);
    }

    g_strfreev (str_layouts);
  }

  return num_layouts;
}

/**
 * @brief Get layout string of tensor layout.
 * @param layout layout of the tensor
 * @return string of layout in tensor
 */
static const gchar *
gst_tensor_get_layout_string (tensor_layout layout)
{
  switch (layout) {
    case _NNS_LAYOUT_NCHW:
      return "NCHW";
    case _NNS_LAYOUT_NHWC:
      return "NHWC";
    case _NNS_LAYOUT_NONE:
      return "NONE";
    case _NNS_LAYOUT_ANY:
      return "ANY";
    default:
      return NULL;
  }
}

/**
 * @brief Get the string of layout of tensors
 * @param layout layout of the tensors
 * @return string of layouts in tensors
 * @note The returned value should be freed with g_free()
 */
static gchar *
gst_tensors_get_layout_string (const GstTensorsInfo * info,
    tensors_layout layout)
{
  gchar *layout_str = NULL;

  g_return_val_if_fail (info != NULL, NULL);

  if (info->num_tensors > 0) {
    guint i;
    GString *layouts = g_string_new (NULL);

    for (i = 0; i < info->num_tensors; i++) {
      g_string_append (layouts, gst_tensor_get_layout_string (layout[i]));

      if (i < info->num_tensors - 1) {
        g_string_append (layouts, ",");
      }
    }

    layout_str = g_string_free (layouts, FALSE);
  }

  return layout_str;
}

/**
 * @brief to get and register hardware accelerator backend enum
 */
static GType
accl_hw_get_type (void)
    G_GNUC_CONST;

/**
 * @brief copy the string from src to destination
 * @param[in] dest destination string
 * @param[in] src source string
 * @return updated destination string
 */
     static gchar *strcpy2 (gchar * dest, const gchar * src)
{
  memcpy (dest, src, strlen (src));
  return dest + strlen (src);
}

/**
 * @brief create regex for the given string list and regex basic elements
 * @param[in] enum_list list of strings to form regex for
 * @param[in] regex_utils list of basic elements to form regex
 * @return the formed regex (to be freed by the caller), NULL on error
 */
static gchar *
create_regex (const gchar ** enum_list, const gchar ** regex_utils)
{
  gchar regex[4096];
  gchar *regex_ptr = regex;
  const gchar **strings = enum_list;
  const gchar *iterator = *strings;
  const gchar *escape_separator = "\\.";
  const gchar *escape_chars = ".";
  gchar **regex_split;
  gchar *regex_escaped;

  if (iterator == NULL)
    return NULL;

  /** create the regex string */
  regex_ptr = strcpy2 (regex_ptr, regex_utils[0]);
  regex_ptr = strcpy2 (regex_ptr, regex_utils[1]);
  regex_ptr = strcpy2 (regex_ptr, iterator);
  regex_ptr = strcpy2 (regex_ptr, regex_utils[2]);
  for (iterator = strings[1]; iterator != NULL; iterator = *++strings) {
    regex_ptr = strcpy2 (regex_ptr, regex_utils[3]);
    regex_ptr = strcpy2 (regex_ptr, regex_utils[1]);
    regex_ptr = strcpy2 (regex_ptr, iterator);
    regex_ptr = strcpy2 (regex_ptr, regex_utils[2]);
  }
  regex_ptr = strcpy2 (regex_ptr, regex_utils[4]);
  *regex_ptr = '\0';

  /** escape the special characters */
  regex_split = g_strsplit_set (regex, escape_chars, -1);
  regex_escaped = g_strjoinv (escape_separator, regex_split);
  g_strfreev (regex_split);

  return regex_escaped;
}

/**
 * @brief Verify validity of path for given model file if verify_model_path is set
 * @param[in] priv Struct containing the common tensor-filter properties of the object
 * @return TRUE if there is no error
 */
static inline gboolean
verify_model_path (const GstTensorFilterPrivate * priv)
{
  const GstTensorFilterProperties *prop;
  gboolean ret = TRUE;
  int verify_model_path = 0, i;

  if (priv == NULL)
    return FALSE;

  prop = &(priv->prop);

  if (g_strcmp0 (prop->fwname, "custom-easy") == 0)
    return TRUE;

  if (GST_TF_FW_V0 (priv->fw)) {
    verify_model_path = priv->fw->verify_model_path;
  } else if (GST_TF_FW_V1 (priv->fw)) {
    verify_model_path = priv->info.verify_model_path;
  }

  if ((prop->model_files != NULL) && (verify_model_path == TRUE)) {
    for (i = 0; i < prop->num_models; i++) {
      if (!g_file_test (prop->model_files[i], G_FILE_TEST_IS_REGULAR)) {
        g_critical ("Cannot find the model file [%d]: %s\n",
            i, prop->model_files[i]);
        ret = FALSE;
      }
    }
  }

  return ret;
}

/**
 * @brief Initialize the GstTensorFilterProperties object
 */
static void
gst_tensor_filter_properties_init (GstTensorFilterProperties * prop)
{
  prop->fwname = NULL;
  prop->fw_opened = FALSE;
  prop->model_files = NULL;
  prop->num_models = 0;

  prop->input_configured = FALSE;
  gst_tensors_info_init (&prop->input_meta);
  gst_tensors_layout_init (prop->input_layout);

  prop->output_configured = FALSE;
  gst_tensors_info_init (&prop->output_meta);
  gst_tensors_layout_init (prop->output_layout);

  prop->custom_properties = NULL;
  prop->accl_str = NULL;
}

/**
 * @brief Validate filter sub-plugin's data.
 */
static gboolean
nnstreamer_filter_validate (const GstTensorFilterFramework * tfsp)
{
  if (GST_TF_FW_V0 (tfsp)) {
    if (!tfsp->name) {
      /* invalid fw name */
      return FALSE;
    }

    if (!tfsp->invoke_NN) {
      /* no invoke function */
      return FALSE;
    }

    if (!(tfsp->getInputDimension && tfsp->getOutputDimension) &&
        !tfsp->setInputDimension) {
      /* no method to get tensor info */
      return FALSE;
    }
  } else if (GST_TF_FW_V1 (tfsp)) {
    GstTensorFilterFrameworkInfo info;
    GstTensorFilterProperties prop;

    if (!tfsp->invoke || !tfsp->getFrameworkInfo || !tfsp->getModelInfo ||
        !tfsp->eventHandler) {
      /** Mandatory callbacks are not defined */
      return FALSE;
    }

    gst_tensor_filter_properties_init (&prop);
    if (tfsp->getFrameworkInfo (&prop, NULL, &info) != 0) {
      /* unable to get framework info */
      return FALSE;
    }

    if (!info.name) {
      /* invalid fw name */
      return FALSE;
    }
  } else {
    return FALSE;
  }

  return TRUE;
}

/**
 * @brief Filter's sub-plugin should call this function to register itself.
 * @param[in] tfsp Tensor-Filter Sub-Plugin to be registered.
 * @return TRUE if registered. FALSE is failed or duplicated.
 */
int
nnstreamer_filter_probe (GstTensorFilterFramework * tfsp)
{
  GstTensorFilterFrameworkInfo info;
  GstTensorFilterProperties prop;
  char *name = NULL;

  g_return_val_if_fail (nnstreamer_filter_validate (tfsp), FALSE);

  if (GST_TF_FW_V0 (tfsp)) {
    name = tfsp->name;
  } else if (GST_TF_FW_V1 (tfsp)) {
    gst_tensor_filter_properties_init (&prop);
    g_assert (tfsp->getFrameworkInfo (&prop, NULL, &info) == 0);
    name = info.name;
  }

  return register_subplugin (NNS_SUBPLUGIN_FILTER, name, tfsp);
}

/**
 * @brief Filter's sub-plugin may call this to unregister itself.
 * @param[in] name The name of filter sub-plugin.
 */
void
nnstreamer_filter_exit (const char *name)
{
  unregister_subplugin (NNS_SUBPLUGIN_FILTER, name);
}

/**
 * @brief Find filter sub-plugin with the name.
 * @param[in] name The name of filter sub-plugin.
 * @return NULL if not found or the sub-plugin object has an error.
 */
const GstTensorFilterFramework *
nnstreamer_filter_find (const char *name)
{
  return get_subplugin (NNS_SUBPLUGIN_FILTER, name);
}

/**
 * @brief Parse the string of model
 * @param[out] prop Struct containing the properties of the object
 * @param[in] model_files the prediction model paths
 */
static void
gst_tensor_filter_parse_modelpaths_string (GstTensorFilterProperties * prop,
    const gchar * model_files)
{
  if (prop == NULL)
    return;

  g_strfreev_const (prop->model_files);

  if (model_files) {
    prop->model_files = (const gchar **) g_strsplit_set (model_files, ",", -1);
    prop->num_models = g_strv_length ((gchar **) prop->model_files);
  } else {
    prop->model_files = NULL;
    prop->num_models = 0;
  }
}

/**
 * @brief check if the allocate_in_invoke is valid for the framework
 * @param[in] priv Struct containing the properties of the object
 * @return TRUE if valid, FALSE on error
 */
gboolean
gst_tensor_filter_allocate_in_invoke (GstTensorFilterPrivate * priv)
{
  int allocate_in_invoke = 0;

  if (GST_TF_FW_V0 (priv->fw)) {
    allocate_in_invoke = priv->fw->allocate_in_invoke;
    if (allocate_in_invoke == TRUE && priv->fw->allocateInInvoke) {
      if (priv->fw->allocateInInvoke (&priv->privateData) == 0) {
        allocate_in_invoke = TRUE;
      } else {
        allocate_in_invoke = FALSE;
      }
    }
  } else if (GST_TF_FW_V1 (priv->fw)) {
    allocate_in_invoke = priv->info.allocate_in_invoke;
  }

  return allocate_in_invoke;
}

/**
 * @brief Printout the comparison results of two tensors.
 * @param[in] info1 The tensors to be shown on the left hand side
 * @param[in] info2 The tensors to be shown on the right hand side
 */
void
gst_tensor_filter_compare_tensors (GstTensorsInfo * info1,
    GstTensorsInfo * info2)
{
  gchar *result = NULL;
  gchar *line, *tmp, *left, *right;
  guint i;

  for (i = 0; i < NNS_TENSOR_SIZE_LIMIT; i++) {
    if (info1->num_tensors <= i && info2->num_tensors <= i)
      break;

    if (info1->num_tensors > i) {
      tmp = gst_tensor_get_dimension_string (info1->info[i].dimension);
      left = g_strdup_printf ("%s [%s]",
          gst_tensor_get_type_string (info1->info[i].type), tmp);
      g_free (tmp);
    } else {
      left = g_strdup ("None");
    }

    if (info2->num_tensors > i) {
      tmp = gst_tensor_get_dimension_string (info2->info[i].dimension);
      right = g_strdup_printf ("%s [%s]",
          gst_tensor_get_type_string (info2->info[i].type), tmp);
      g_free (tmp);
    } else {
      right = g_strdup ("None");
    }

    line =
        g_strdup_printf ("%2d : %s | %s %s\n", i, left, right,
        g_str_equal (left, right) ? "" : "FAILED");

    g_free (left);
    g_free (right);

    if (result) {
      tmp = g_strdup_printf ("%s%s", result, line);
      g_free (result);
      g_free (line);

      result = tmp;
    } else {
      result = line;
    }
  }

  if (result) {
    /* print warning message */
    nns_logw ("Tensor info :\n%s", result);
    g_free (result);
  }
}

/**
 * @brief Installs all the properties for tensor_filter
 * @param[in] gobject_class Glib object class whose properties will be set
 */
void
gst_tensor_filter_install_properties (GObjectClass * gobject_class)
{
  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_FRAMEWORK,
      g_param_spec_string ("framework", "Framework",
          "Neural network framework", "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MODEL,
      g_param_spec_string ("model", "Model filepath",
          "File path to the model file. Separated with ',' in case of multiple model files(like caffe2)",
          "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INPUT,
      g_param_spec_string ("input", "Input dimension",
          "Input tensor dimension from inner array, up to 4 dimensions ?", "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INPUTNAME,
      g_param_spec_string ("inputname", "Name of Input Tensor",
          "The Name of Input Tensor", "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INPUTTYPE,
      g_param_spec_string ("inputtype", "Input tensor element type",
          "Type of each element of the input tensor ?", "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INPUTLAYOUT,
      g_param_spec_string ("inputlayout", "Input Data Layout",
          "Set channel first (NCHW) or channel last layout (NHWC) or None for input data. "
          "Layout of the data can be any or NHWC or NCHW or none for now. ",
          "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUTPUTNAME,
      g_param_spec_string ("outputname", "Name of Output Tensor",
          "The Name of Output Tensor", "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUTPUT,
      g_param_spec_string ("output", "Output dimension",
          "Output tensor dimension from inner array, up to 4 dimensions ?", "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUTPUTTYPE,
      g_param_spec_string ("outputtype", "Output tensor element type",
          "Type of each element of the output tensor ?", "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUTPUTLAYOUT,
      g_param_spec_string ("outputlayout", "Output Data Layout",
          "Set channel first (NCHW) or channel last layout (NHWC) or None for output data. "
          "Layout of the data can be any or NHWC or NCHW or none for now. ",
          "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CUSTOM,
      g_param_spec_string ("custom", "Custom properties for subplugins",
          "Custom properties for subplugins ?", "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SUBPLUGINS,
      g_param_spec_string ("sub-plugins", "Sub-plugins",
          "Registrable sub-plugins list", "",
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ACCELERATOR,
      g_param_spec_string ("accelerator", "ACCELERATOR",
          "Set accelerator for the subplugin with format "
          "(true/false):(comma separated ACCELERATOR(s)). "
          "true/false determines if accelerator is to be used. "
          "list of accelerators determines the backend (ignored with false). "
          "Example, if GPU, NPU can be used but not CPU - true:(GPU,NPU,!CPU). "
          "Note that only a few subplugins support this property.",
          "", G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_IS_UPDATABLE,
      g_param_spec_boolean ("is-updatable", "Updatable model",
          "Indicate whether a given model to this tensor filter is "
          "updatable in runtime. (e.g., with on-device training)",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/**
 * @brief Initialize the properties for tensor-filter.
 */
void
gst_tensor_filter_common_init_property (GstTensorFilterPrivate * priv)
{
  /* init NNFW properties */
  gst_tensor_filter_properties_init (&priv->prop);
  priv->info.name = NULL;

  /* init internal properties */
  priv->fw = NULL;
  priv->privateData = NULL;
  priv->silent = TRUE;
  priv->configured = FALSE;
  gst_tensors_config_init (&priv->in_config);
  gst_tensors_config_init (&priv->out_config);
}

/**
 * @brief Free the properties for tensor-filter.
 */
void
gst_tensor_filter_common_free_property (GstTensorFilterPrivate * priv)
{
  GstTensorFilterProperties *prop;

  prop = &priv->prop;

  g_free_const (prop->fwname);
  if (GST_TF_FW_V0 (priv->fw)) {
    g_free_const (prop->accl_str);
  } else if (GST_TF_FW_V1 (priv->fw)) {
    g_free (prop->hw_list);
  }
  g_free_const (prop->custom_properties);
  g_strfreev_const (prop->model_files);

  gst_tensors_info_free (&prop->input_meta);
  gst_tensors_info_free (&prop->output_meta);

  gst_tensors_info_free (&priv->in_config.info);
  gst_tensors_info_free (&priv->out_config.info);
}

/**
 * @brief Parse the accelerator hardwares to be used for this framework
 * @param[in] priv Struct containing the properties of the object
 * @param[in] prop Struct containing the properties of the framework
 * @param[in] accelerators user given input for hardare accelerators
 * @note The order of preference set by the user is maintained
 */
static void
gst_tensor_filter_parse_accelerator (GstTensorFilterPrivate * priv,
    GstTensorFilterProperties * prop, const char *accelerators)
{
  gint status, idx;
  GstTensorFilterFrameworkInfo *info;
  const gchar **accl_support;
  GList *match_accl, *iter;

  info = &priv->info;
  prop->num_hw = 0;
  g_free (prop->hw_list);
  prop->hw_list = NULL;

  /** Get h/w accelerators supported by framework */
  if (info->name == NULL) {
    status = priv->fw->getFrameworkInfo (prop, priv->privateData, info);
    if (status != 0 || info->hw_list == NULL) {
      g_warning ("Unable to fetch accelerators supported by the framework.");
      return;
    }
  }

  /** Convert the list to string format */
  accl_support = g_malloc (sizeof (gchar *) * (info->num_hw + 1));
  if (!accl_support)
    return;

  for (idx = 0; idx < info->num_hw; idx++) {
    accl_support[idx] = get_accl_hw_str (info->hw_list[idx]);
  }
  accl_support[info->num_hw] = NULL;

  /** Parse the user given h/w accelerators intersected with supported h/w */
  match_accl = parse_accl_hw_all (accelerators, accl_support);
  g_free (accl_support);

  /** Convert the GList to regular array */
  prop->num_hw = g_list_length (match_accl);
  prop->hw_list = g_malloc (sizeof (accl_hw) * prop->num_hw);
  for (iter = match_accl, idx = 0; iter != NULL; iter = iter->next, idx++) {
    prop->hw_list[idx] = GPOINTER_TO_INT (iter->data);
  }
  g_list_free (match_accl);
}

/**
 * @brief Set the properties for tensor_filter
 * @param[in] priv Struct containing the properties of the object
 * @param[in] prop_id Id for the property
 * @param[in] value Container to return the asked property
 * @param[in] pspec Metadata to specify the parameter
 * @return TRUE if prop_id is value, else FALSE
 */
gboolean
gst_tensor_filter_common_set_property (GstTensorFilterPrivate * priv,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  gint status = 0;
  GstTensorFilterProperties *prop;

  prop = &priv->prop;

  switch (prop_id) {
    case PROP_SILENT:
      priv->silent = g_value_get_boolean (value);
      break;
    case PROP_FRAMEWORK:
    {
      const gchar *fw_name = g_value_get_string (value);
      const GstTensorFilterFramework *fw;

      if (priv->fw != NULL) {
        if (g_strcmp0 (priv->prop.fwname, fw_name) != 0) {
          /* close old framework, if different */
          gst_tensor_filter_common_close_fw (priv);
          priv->fw = NULL;
        } else {
          g_debug ("Framework = %s\n", fw_name);
          break;
        }
      }

      g_debug ("Framework = %s\n", fw_name);

      fw = nnstreamer_filter_find (fw_name);

      if (fw) {
        /** Get framework info for v1 */
        if (GST_TF_FW_V1 (fw) &&
            fw->getFrameworkInfo (prop, NULL, &priv->info) < 0) {
          ml_logw ("Cannot get the given framework info, %s\n", fw_name);
          break;
        }
        priv->fw = fw;
        prop->fwname = g_strdup (fw_name);

        /** update the accelerator if already set based on v0 or v1 */
        if (GST_TF_FW_V1 (priv->fw)) {
          if (prop->accl_str) {
            const gchar *accl_str = prop->accl_str;
            gst_tensor_filter_parse_accelerator (priv, &priv->prop, accl_str);
            g_free_const (accl_str);
          } else {
            prop->hw_list = NULL;
          }
        }
      } else {
        ml_logw ("Cannot identify the given neural network framework, %s\n",
            fw_name);
      }
      break;
    }
    case PROP_MODEL:
    {
      const gchar *model_files = g_value_get_string (value);
      GstTensorFilterProperties _prop;

      if (!model_files) {
        ml_loge ("Invalid model provided to the tensor-filter.");
        break;
      }
      _prop.model_files = NULL;

      if (prop->fw_opened) {
        /** Store a copy of the original prop in case the reload fails */
        memcpy (&_prop, prop, sizeof (GstTensorFilterProperties));
        _prop.model_files =
            (const gchar **) g_strdupv ((gchar **) prop->model_files);
      }

      gst_tensor_filter_parse_modelpaths_string (prop, model_files);

      /**
       * Reload model if FW has been already opened;
       * In the case of reloading model files, each priv->fw (tensor filter for each nnfw)
       * has responsibility for the verification of the path regardless of priv->fw->verify_model_path.
       */
      if (prop->fw_opened) {
        if (GST_TF_FW_V0 (priv->fw) && priv->is_updatable) {
          if (priv->fw->reloadModel &&
              priv->fw->reloadModel (prop, &priv->privateData) != 0) {
            status = -1;
          }
        } else if (GST_TF_FW_V1 (priv->fw) && priv->is_updatable) {
          GstTensorFilterFrameworkEventData data;
          data.model_files = prop->model_files;
          data.num_models = prop->num_models;
          /** original prop is sent and not the updated prop */
          if (priv->fw->eventHandler (&_prop, priv->privateData, RELOAD_MODEL,
                  &data) != 0) {
            status = -1;
          }
        }

        if (status == 0) {
          g_strfreev_const (_prop.model_files);
        } else {
          g_critical ("Fail to reload model\n");
          g_strfreev_const (prop->model_files);
          prop->model_files = _prop.model_files;
          prop->num_models = _prop.num_models;
        }
      }

      break;
    }
    case PROP_INPUT:
      if (!prop->input_configured && value) {
        guint num_dims;

        num_dims = gst_tensors_info_parse_dimensions_string (&prop->input_meta,
            g_value_get_string (value));

        if (prop->input_meta.num_tensors > 0 &&
            prop->input_meta.num_tensors != num_dims) {
          ml_logw
              ("Invalid input-dim, given param does not match with old value.");
        }

        prop->input_meta.num_tensors = num_dims;
      } else if (value) {
        /** Once configured, it cannot be changed in runtime for now */
        ml_loge
            ("Cannot change input-dim once the element/pipeline is configured.");
      }
      break;
    case PROP_OUTPUT:
      if (!prop->output_configured && value) {
        guint num_dims;

        num_dims = gst_tensors_info_parse_dimensions_string (&prop->output_meta,
            g_value_get_string (value));

        if (prop->output_meta.num_tensors > 0 &&
            prop->output_meta.num_tensors != num_dims) {
          ml_logw
              ("Invalid output-dim, given param does not match with old value.");
        }

        prop->output_meta.num_tensors = num_dims;
      } else if (value) {
        /** Once configured, it cannot be changed in runtime for now */
        ml_loge
            ("Cannot change output-dim once the element/pipeline is configured.");
      }
      break;
    case PROP_INPUTTYPE:
      if (!prop->input_configured && value) {
        guint num_types;

        num_types = gst_tensors_info_parse_types_string (&prop->input_meta,
            g_value_get_string (value));

        if (prop->input_meta.num_tensors > 0 &&
            prop->input_meta.num_tensors != num_types) {
          ml_logw
              ("Invalid input-type, given param does not match with old value.");
        }

        prop->input_meta.num_tensors = num_types;
      } else if (value) {
        /** Once configured, it cannot be changed in runtime for now */
        ml_loge
            ("Cannot change input-type once the element/pipeline is configured.");
      }
      break;
    case PROP_OUTPUTTYPE:
      if (!prop->output_configured && value) {
        guint num_types;

        num_types = gst_tensors_info_parse_types_string (&prop->output_meta,
            g_value_get_string (value));

        if (prop->output_meta.num_tensors > 0 &&
            prop->output_meta.num_tensors != num_types) {
          ml_logw
              ("Invalid output-type, given param does not match with old value.");
        }

        prop->output_meta.num_tensors = num_types;
      } else if (value) {
        /** Once configured, it cannot be changed in runtime for now */
        ml_loge
            ("Cannot change output-type once the element/pipeline is configured.");
      }
      break;
    case PROP_INPUTNAME:
      /* INPUTNAME is required by tensorflow to designate the order of tensors */
      if (!prop->input_configured && value) {
        guint num_names;

        num_names = gst_tensors_info_parse_names_string (&prop->input_meta,
            g_value_get_string (value));

        if (prop->input_meta.num_tensors > 0 &&
            prop->input_meta.num_tensors != num_names) {
          ml_logw
              ("Invalid input-name, given param does not match with old value.");
        }

        prop->input_meta.num_tensors = num_names;
      } else if (value) {
        /** Once configured, it cannot be changed in runtime for now */
        ml_loge
            ("Cannot change input-name once the element/pipeline is configured.");
      }
      break;
    case PROP_OUTPUTNAME:
      /* OUTPUTNAME is required by tensorflow to designate the order of tensors */
      if (!prop->output_configured && value) {
        guint num_names;

        num_names = gst_tensors_info_parse_names_string (&prop->output_meta,
            g_value_get_string (value));

        if (prop->output_meta.num_tensors > 0 &&
            prop->output_meta.num_tensors != num_names) {
          ml_logw
              ("Invalid output-name, given param does not match with old value.");
        }

        prop->output_meta.num_tensors = num_names;
      } else if (value) {
        /** Once configured, it cannot be changed in runtime for now */
        ml_loge
            ("Cannot change output-name once the element/pipeline is configured.");
      }
      break;
    case PROP_CUSTOM:
    {
      if (!priv->prop.fw_opened) {
        g_free_const (prop->custom_properties);
        prop->custom_properties = g_value_dup_string (value);
      } else {
        if (GST_TF_FW_V0 (priv->fw)) {
          ml_loge
              ("Cannot change custom-prop once the element/pipeline is configured.");
        } else if (GST_TF_FW_V1 (priv->fw)) {
          GstTensorFilterFrameworkEventData data;

          data.custom_properties = g_value_dup_string (value);
          status = priv->fw->eventHandler
              (prop, &priv->privateData, CUSTOM_PROP, &data);
          if (status == 0) {
            g_free_const (prop->custom_properties);
            prop->custom_properties = g_value_dup_string (value);
          }

          g_free_const (data.custom_properties);
        }
      }

      break;
    }
    case PROP_ACCELERATOR:
    {
      gchar *accelerators = g_value_dup_string (value);

      if (priv->prop.fw_opened == TRUE) {
        if (GST_TF_FW_V0 (priv->fw)) {
          ml_loge
              ("Cannot change accelerator once the element/pipeline is configured.");
        } else if (GST_TF_FW_V1 (priv->fw)) {
          GstTensorFilterProperties _prop;
          GstTensorFilterFrameworkEventData data;
          memcpy (&_prop, prop, sizeof (GstTensorFilterProperties));

          prop->hw_list = NULL;
          gst_tensor_filter_parse_accelerator (priv, prop, accelerators);
          data.num_hw = prop->num_hw;
          data.hw_list = prop->hw_list;

          status = priv->fw->eventHandler
              (&_prop, priv->privateData, SET_ACCELERATOR, &data);
          if (status == 0) {
            g_free (_prop.hw_list);
          } else {
            prop->num_hw = _prop.num_hw;
            g_free (prop->hw_list);
            prop->hw_list = _prop.hw_list;
          }

          g_free (accelerators);
        }
        break;
      }

      if (GST_TF_FW_V0 (priv->fw)) {
        prop->accl_str = accelerators;
      } else if (GST_TF_FW_V1 (priv->fw)) {
        gst_tensor_filter_parse_accelerator (priv, &priv->prop, accelerators);
        g_free (accelerators);
      }
      break;
    }
    case PROP_IS_UPDATABLE:
    {
      if (GST_TF_FW_V0 (priv->fw) && priv->fw->reloadModel == NULL) {
        break;
      } else if (GST_TF_FW_V1 (priv->fw) &&
          priv->fw->eventHandler (prop, priv->privateData, RELOAD_MODEL, NULL)
          == -ENOENT) {
        break;
      }

      priv->is_updatable = g_value_get_boolean (value);
      break;
    }
    case PROP_INPUTLAYOUT:
    {
      guint num_layouts;

      if (!prop->output_configured && value) {
        num_layouts = gst_tensors_parse_layouts_string (prop->input_layout,
            g_value_get_string (value));

        if (prop->input_meta.num_tensors > 0 &&
            prop->input_meta.num_tensors != num_layouts) {
          ml_logw ("Invalid input-layout, given param does not fit.");
        }

        prop->input_meta.num_tensors = num_layouts;
      } else if (value) {
        /** Update the properties */
        if (GST_TF_FW_V0 (priv->fw)) {
          /* Once configured, it cannot be changed in runtime */
          ml_loge
              ("Cannot change input-layout once the element/pipeline is configured.");
        } else if (GST_TF_FW_V1 (priv->fw)) {
          GstTensorFilterFrameworkEventData data;

          data.info = NULL;
          num_layouts = gst_tensors_parse_layouts_string (data.layout,
              g_value_get_string (value));

          if (prop->input_meta.num_tensors > 0 &&
              prop->input_meta.num_tensors != num_layouts) {
            ml_logw ("Invalid input-layout, given param does not fit.");
          }

          if (priv->fw->eventHandler
              (prop, priv->privateData, SET_INPUT_PROP, &data) == 0) {
            memcpy (priv->prop.input_layout, data.layout,
                sizeof (tensor_layout) * NNS_TENSOR_SIZE_LIMIT);
          } else {
            ml_logw ("Unable to update input layout.");
          }
        }
      }
      break;
    }
    case PROP_OUTPUTLAYOUT:
    {
      guint num_layouts;

      if (!prop->output_configured && value) {
        num_layouts = gst_tensors_parse_layouts_string (prop->output_layout,
            g_value_get_string (value));

        if (prop->output_meta.num_tensors > 0 &&
            prop->output_meta.num_tensors != num_layouts) {
          ml_logw ("Invalid output-layout, given param does not fit.");
        }

        prop->output_meta.num_tensors = num_layouts;
      } else if (value) {
        /** Update the properties */
        if (GST_TF_FW_V0 (priv->fw)) {
          /* Once configured, it cannot be changed in runtime */
          ml_loge
              ("Cannot change output-layout once the element/pipeline is configured.");
        } else if (GST_TF_FW_V1 (priv->fw)) {
          GstTensorFilterFrameworkEventData data;

          data.info = NULL;
          num_layouts = gst_tensors_parse_layouts_string (data.layout,
              g_value_get_string (value));

          if (prop->output_meta.num_tensors > 0 &&
              prop->output_meta.num_tensors != num_layouts) {
            ml_logw ("Invalid output-layout, given param does not fit.");
          }

          if (priv->fw->eventHandler
              (prop, priv->privateData, SET_OUTPUT_PROP, &data) == 0) {
            memcpy (priv->prop.output_layout, data.layout,
                sizeof (tensor_layout) * NNS_TENSOR_SIZE_LIMIT);
          } else {
            ml_logw ("Unable to update output layout.");
          }
        }
      }
      break;
    }
    default:
      return FALSE;
  }

  return TRUE;
}

/**
 * @brief Get the properties for tensor_filter
 * @param[in] priv Struct containing the properties of the object
 * @param[in] prop_id Id for the property
 * @param[in] value Container to return the asked property
 * @param[in] pspec Metadata to specify the parameter
 * @return TRUE if prop_id is value, else FALSE
 */
gboolean
gst_tensor_filter_common_get_property (GstTensorFilterPrivate * priv,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstTensorFilterProperties *prop;

  prop = &priv->prop;

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, priv->silent);
      break;
    case PROP_FRAMEWORK:
      g_value_set_string (value, prop->fwname);
      break;
    case PROP_MODEL:
    {
      GString *gstr_models = g_string_new (NULL);
      gchar *models;
      int idx;

      /* return a comma-separated string */
      for (idx = 0; idx < prop->num_models; ++idx) {
        if (idx != 0) {
          g_string_append (gstr_models, ",");
        }

        g_string_append (gstr_models, prop->model_files[idx]);
      }

      models = g_string_free (gstr_models, FALSE);
      g_value_take_string (value, models);
      break;
    }
    case PROP_INPUT:
      if (prop->input_meta.num_tensors > 0) {
        gchar *dim_str;

        dim_str = gst_tensors_info_get_dimensions_string (&prop->input_meta);

        g_value_set_string (value, dim_str);
        g_free (dim_str);
      } else {
        g_value_set_string (value, "");
      }
      break;
    case PROP_OUTPUT:
      if (prop->output_meta.num_tensors > 0) {
        gchar *dim_str;

        dim_str = gst_tensors_info_get_dimensions_string (&prop->output_meta);

        g_value_set_string (value, dim_str);
        g_free (dim_str);
      } else {
        g_value_set_string (value, "");
      }
      break;
    case PROP_INPUTTYPE:
      if (prop->input_meta.num_tensors > 0) {
        gchar *type_str;

        type_str = gst_tensors_info_get_types_string (&prop->input_meta);

        g_value_set_string (value, type_str);
        g_free (type_str);
      } else {
        g_value_set_string (value, "");
      }
      break;
    case PROP_OUTPUTTYPE:
      if (prop->output_meta.num_tensors > 0) {
        gchar *type_str;

        type_str = gst_tensors_info_get_types_string (&prop->output_meta);

        g_value_set_string (value, type_str);
        g_free (type_str);
      } else {
        g_value_set_string (value, "");
      }
      break;
    case PROP_INPUTNAME:
      if (prop->input_meta.num_tensors > 0) {
        gchar *name_str;

        name_str = gst_tensors_info_get_names_string (&prop->input_meta);

        g_value_set_string (value, name_str);
        g_free (name_str);
      } else {
        g_value_set_string (value, "");
      }
      break;
    case PROP_OUTPUTNAME:
      if (prop->output_meta.num_tensors > 0) {
        gchar *name_str;

        name_str = gst_tensors_info_get_names_string (&prop->output_meta);

        g_value_set_string (value, name_str);
        g_free (name_str);
      } else {
        g_value_set_string (value, "");
      }
      break;
    case PROP_CUSTOM:
      g_value_set_string (value, prop->custom_properties);
      break;
    case PROP_SUBPLUGINS:
    {
      GString *subplugins;
      subplugin_info_s sinfo;
      guint i, total;

      subplugins = g_string_new (NULL);

      /* add custom */
      /** @todo Let's not hardcode default subplugins */
      g_string_append (subplugins, "custom,custom-easy");

      total = nnsconf_get_subplugin_info (NNSCONF_PATH_FILTERS, &sinfo);

      if (total > 0) {
        const gchar *prefix_str;
        gsize prefix, extension, len;

        prefix_str = nnsconf_get_subplugin_name_prefix (NNSCONF_PATH_FILTERS);
        prefix = strlen (prefix_str);
        extension = strlen (NNSTREAMER_SO_FILE_EXTENSION);

        for (i = 0; i < total; ++i) {
          g_string_append (subplugins, ",");

          /* remove file extension */
          len = strlen (sinfo.names[i]) - prefix - extension;
          g_string_append_len (subplugins, sinfo.names[i] + prefix, len);
        }
      }

      g_value_take_string (value, g_string_free (subplugins, FALSE));
      break;
    }
    case PROP_ACCELERATOR:
    {
      gint idx;
      GString *accl;

      if (priv->fw == NULL || GST_TF_FW_V0 (priv->fw)) {
        if (prop->accl_str != NULL) {
          g_value_set_string (value, prop->accl_str);
        } else {
          g_value_set_string (value, "");
        }
      } else if (GST_TF_FW_V1 (priv->fw)) {
        if (prop->num_hw == 0) {
          g_value_set_string (value, "");
        } else {
          accl = g_string_new (NULL);

          for (idx = 0; idx < prop->num_hw; idx++) {
            g_string_append (accl, get_accl_hw_str (prop->hw_list[idx]));
          }
          g_value_take_string (value, g_string_free (accl, FALSE));
        }
      }
      break;
    }
    case PROP_IS_UPDATABLE:
      g_value_set_boolean (value, priv->is_updatable);
      break;
    case PROP_INPUTLAYOUT:
      if (prop->input_meta.num_tensors > 0) {
        gchar *layout_str;

        layout_str = gst_tensors_get_layout_string (&prop->input_meta,
            prop->input_layout);

        g_value_set_string (value, layout_str);
        g_free (layout_str);
      } else {
        g_value_set_string (value, "");
      }
      break;
    case PROP_OUTPUTLAYOUT:
      if (prop->output_meta.num_tensors > 0) {
        gchar *layout_str;

        layout_str = gst_tensors_get_layout_string (&prop->output_meta,
            prop->output_layout);

        g_value_set_string (value, layout_str);
        g_free (layout_str);
      } else {
        g_value_set_string (value, "");
      }
      break;
    default:
      /* unknown property */
      return FALSE;
  }

  return TRUE;
}

/**
 * @brief Open NN framework.
 */
void
gst_tensor_filter_common_open_fw (GstTensorFilterPrivate * priv)
{
  int run_without_model = 0;

  if (!priv->prop.fw_opened && priv->fw) {
    if (priv->fw->open) {
      /* at least one model should be configured before opening fw */
      if (GST_TF_FW_V0 (priv->fw)) {
        run_without_model = priv->fw->run_without_model;
      } else if (GST_TF_FW_V1 (priv->fw)) {
        run_without_model = priv->info.run_without_model;
      }

      if (G_UNLIKELY (!run_without_model) &&
          G_UNLIKELY (!(priv->prop.model_files &&
                  priv->prop.num_models > 0 && priv->prop.model_files[0]))) {
        return;
      }
      /* 0 if successfully loaded. 1 if skipped (already loaded). */
      if (verify_model_path (priv)) {
        if (priv->fw->open (&priv->prop, &priv->privateData) >= 0) {
          /* Update the framework info once it has been opened */
          if (GST_TF_FW_V1 (priv->fw) &&
              priv->fw->getFrameworkInfo (&priv->prop, priv->privateData,
                  &priv->info) != 0) {
            priv->fw->close (&priv->prop, &priv->privateData);
          } else {
            priv->prop.fw_opened = TRUE;
          }
        }
      }
    } else {
      priv->prop.fw_opened = TRUE;
    }
  }
}

/**
 * @brief Close NN framework.
 */
void
gst_tensor_filter_common_close_fw (GstTensorFilterPrivate * priv)
{
  if (priv->prop.fw_opened) {
    if (priv->fw && priv->fw->close) {
      priv->fw->close (&priv->prop, &priv->privateData);
    }
    priv->prop.input_configured = priv->prop.output_configured = FALSE;
    priv->prop.fw_opened = FALSE;
    g_free_const (priv->prop.fwname);
    priv->prop.fwname = NULL;
    priv->fw = NULL;
    priv->privateData = NULL;
  }
}

/**
 * @brief return accl_hw type from string
 * @param key The key string value
 * @return Corresponding index. Returns ACCL_NONE if not found.
 */
accl_hw
get_accl_hw_type (const gchar * key)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (accl_hw_get_type ());
  enum_value = g_enum_get_value_by_name (enum_class, key);
  g_type_class_unref (enum_class);

  if (enum_value == NULL)
    return ACCL_NONE;
  return enum_value->value;
}

/**
 * @brief return string based on accl_hw type
 * @param key The key enum value
 * @return Corresponding string. Returns ACCL_NONE_STR if not found.
 * @note Do not free the returned char *
 */
const gchar *
get_accl_hw_str (const accl_hw key)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (accl_hw_get_type ());
  enum_value = g_enum_get_value (enum_class, key);
  g_type_class_unref (enum_class);

  if (enum_value == NULL)
    return ACCL_NONE_STR;
  return enum_value->value_name;
}

/**
 * @brief parse user given string to extract list of accelerators based on given regex
 * @param[in] accelerators user given input
 * @param[in] supported_accelerators list of supported accelerators
 * @return Corresponding list of accelerators maintaining given order
 * @note Returned list must be freed by the caller
 */
static GList *
parse_accl_hw_all (const gchar * accelerators,
    const gchar ** supported_accelerators)
{
  GRegex *nnapi_elem;
  GMatchInfo *match_info;
  gboolean use_accl;
  accl_hw accl;
  gchar *regex_accl = NULL;
  gchar *regex_accl_elem = NULL;
  GList *match_accl = NULL;

  if (accelerators == NULL) {
    match_accl = g_list_append (match_accl, GINT_TO_POINTER (ACCL_DEFAULT));
    return match_accl;
  }

  /* If set by user, get the precise accelerator */
  regex_accl = create_regex (supported_accelerators, regex_accl_utils);
  use_accl = (gboolean) g_regex_match_simple (regex_accl, accelerators,
      G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY);
  g_free (regex_accl);
  if (use_accl == TRUE) {
    /** Default to auto mode */
    accl = ACCL_AUTO;
    regex_accl_elem =
        create_regex (supported_accelerators, regex_accl_elem_utils);
    nnapi_elem =
        g_regex_new (regex_accl_elem, G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY,
        NULL);
    g_free (regex_accl_elem);

    /** Now match each provided element and get specific accelerator */
    if (g_regex_match (nnapi_elem, accelerators, G_REGEX_MATCH_NOTEMPTY,
            &match_info)) {

      while (g_match_info_matches (match_info)) {
        gchar *word = g_match_info_fetch (match_info, 0);
        accl = get_accl_hw_type (word);
        g_free (word);
        match_accl = g_list_append (match_accl, GINT_TO_POINTER (accl));
        g_match_info_next (match_info, NULL);
      }
    }
    g_match_info_free (match_info);
    g_regex_unref (nnapi_elem);

    if (g_list_length (match_accl) == 0) {
      match_accl = g_list_append (match_accl, GINT_TO_POINTER (ACCL_AUTO));
    }
  } else {
    match_accl = g_list_append (match_accl, GINT_TO_POINTER (ACCL_NONE));
  }

  return match_accl;
}

/**
 * @brief parse user given string to extract accelerator based on given regex
 * @param[in] accelerators user given input
 * @param[in] supported_accelerators list ofi supported accelerators
 * @return Corresponding accelerator. Returns ACCL_NONE if not found.
 */
accl_hw
parse_accl_hw (const gchar * accelerators,
    const gchar ** supported_accelerators)
{
  GList *match_accl;
  accl_hw hw;

  match_accl = parse_accl_hw_all (accelerators, supported_accelerators);
  g_assert (g_list_length (match_accl) > 0);

  hw = GPOINTER_TO_INT (match_accl->data);
  g_list_free (match_accl);

  return hw;
}

/**
 * @brief to get and register hardware accelerator backend enum
 */
static GType
accl_hw_get_type (void)
{
  static volatile gsize g_accl_hw_type_id__volatile = 0;

  if (g_once_init_enter (&g_accl_hw_type_id__volatile)) {
    static const GEnumValue values[] = {
      {ACCL_NONE, ACCL_NONE_STR, ACCL_NONE_STR},
      {ACCL_DEFAULT, ACCL_DEFAULT_STR, ACCL_DEFAULT_STR},
      {ACCL_AUTO, ACCL_AUTO_STR, ACCL_AUTO_STR},
      {ACCL_CPU, ACCL_CPU_STR, ACCL_CPU_STR},
      {ACCL_CPU_NEON, ACCL_CPU_NEON_STR, ACCL_CPU_NEON_STR},
      {ACCL_GPU, ACCL_GPU_STR, ACCL_GPU_STR},
      {ACCL_NPU, ACCL_NPU_STR, ACCL_NPU_STR},
      {ACCL_NPU_MOVIDIUS, ACCL_NPU_MOVIDIUS_STR, ACCL_NPU_MOVIDIUS_STR},
      {ACCL_NPU_EDGE_TPU, ACCL_NPU_EDGE_TPU_STR, ACCL_NPU_EDGE_TPU_STR},
      {ACCL_NPU_VIVANTE, ACCL_NPU_VIVANTE_STR, ACCL_NPU_VIVANTE_STR},
      {ACCL_NPU_SRCN, ACCL_NPU_SRCN_STR, ACCL_NPU_SRCN_STR},
      {ACCL_NPU_SR, ACCL_NPU_SR_STR, ACCL_NPU_SR_STR},
      {0, NULL, NULL}
    };

    GType g_accl_hw_type_id =
        g_enum_register_static (g_intern_static_string ("accl_hw"), values);
    g_once_init_leave (&g_accl_hw_type_id__volatile, g_accl_hw_type_id);
  }

  return g_accl_hw_type_id__volatile;
}
