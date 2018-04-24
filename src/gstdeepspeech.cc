/*
 * GStreamer DeepSpeech plugin
 * Copyright (C) 2017 Mike Sheldon <elleo@gnu.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-deepspeech
 *
 * Speech recognition plugin suitable for continuous dictation based on
 * Mozilla's DeepSpeech model.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -m pulsesrc ! audioconvert ! audiorate ! audioresample ! deepspeech silence-threshold=0.2 silence-length=20 ! fakesink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <deepspeech.h>
#include <string.h>

#include "gstdeepspeech.h"

GST_DEBUG_CATEGORY_STATIC (gst_deepspeech_debug);
#define GST_CAT_DEFAULT gst_deepspeech_debug

#define N_CEP 26
#define N_CONTEXT 9
#define BEAM_WIDTH 500
#define LM_WEIGHT 1.75f
#define WORD_COUNT_WEIGHT 1.00f
#define VALID_WORD_COUNT_WEIGHT 1.00f

#define DEFAULT_SPEECH_MODEL "/usr/share/deepspeech/models/output_graph.pb"
#define DEFAULT_LANGUAGE_MODEL "/usr/share/deepspeech/models/lm.binary"
#define DEFAULT_TRIE "/usr/share/deepspeech/models/trie"
#define DEFAULT_ALPHABET "/usr/share/deepspeech/models/alphabet.txt"
#define DEFAULT_SILENCE_THRESHOLD 0.1
#define DEFAULT_SILENCE_LENGTH 5


/* Filter signals and args */
enum
{
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SPEECH_MODEL,
  PROP_ALPHABET,
  PROP_LANGUAGE_MODEL,
  PROP_TRIE,
  PROP_SILENCE_THRESHOLD,
  PROP_SILENCE_LENGTH
};

/* the capabilities of the inputs and outputs. */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw,format=S16LE,rate=16000,channels=1")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw,format=S16LE,rate=16000,channels=1")
    );

#define gst_deepspeech_parent_class parent_class
G_DEFINE_TYPE (GstDeepSpeech, gst_deepspeech, GST_TYPE_ELEMENT);

static void gst_deepspeech_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_deepspeech_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_deepspeech_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_deepspeech_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);
static GstMessage * gst_deepspeech_message_new (GstDeepSpeech * deepspeech, GstBuffer * buf, const char * text);
static void gst_deepspeech_load_model (GstDeepSpeech * deepspeech);

static GMutex mutex;

gpointer run_model_async(void * instance_data, void * pool_data)
{
  GstDeepSpeech * deepspeech = GST_DEEPSPEECH (pool_data);
  GstBuffer * buf = GST_BUFFER (instance_data);
  GstMapInfo info;
  float *mfcc;
  int n_frames = 0;
  char *result;

  gst_buffer_map(buf, &info, GST_MAP_READ);
  g_mutex_lock(&mutex);
  deepspeech->model->getInputVector((const short *)info.data, (unsigned int) info.size, 16000, &mfcc, &n_frames);
  result = deepspeech->model->infer(mfcc, n_frames);
  g_mutex_unlock(&mutex);

  if (strlen(result) > 0) {
    GstMessage *msg = gst_deepspeech_message_new (deepspeech, buf, result);
    gst_element_post_message (GST_ELEMENT (deepspeech), msg);
  }

  free(mfcc);
  free(result);
  gst_buffer_unref(buf);

  return NULL;
}

/* GObject vmethod implementations */

/* initialize the deepspeech's class */
static void
gst_deepspeech_class_init (GstDeepSpeechClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_deepspeech_set_property;
  gobject_class->get_property = gst_deepspeech_get_property;

  g_object_class_install_property (gobject_class, PROP_SPEECH_MODEL,
      g_param_spec_string ("speech-model", "Speech Model", "Location of the speech graph file.",
          DEFAULT_SPEECH_MODEL, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_ALPHABET,
      g_param_spec_string ("alphabet", "Alphabet", "Location of the alphabet file corresponding to the speech model.",
          DEFAULT_ALPHABET, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_LANGUAGE_MODEL,
      g_param_spec_string ("language-model", "Language Model", "Location of the language model file.",
          DEFAULT_LANGUAGE_MODEL, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_TRIE,
      g_param_spec_string ("trie", "Trie", "Location of the trie file corresponding to the language model.",
          DEFAULT_TRIE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_SILENCE_THRESHOLD,
      g_param_spec_double ("silence-threshold", "Silence Threshold", "Segment speech when volume is below the threshold for the specified silence length.",
          0, 1.0, DEFAULT_SILENCE_THRESHOLD, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_SILENCE_LENGTH,
      g_param_spec_int ("silence-length", "Silence Length", "Number of buffers which must be below the silence threshold before segmentation occurs.",
          0, G_MAXINT, DEFAULT_SILENCE_LENGTH, G_PARAM_READWRITE));


  gst_element_class_set_details_simple(gstelement_class,
    "deepspeech",
    "Filter/Audio",
    "Performs speech recognition using the Mozilla DeepSpeech model",
    "Mike Sheldon <elleo@gnu.org>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));

  g_mutex_init(&mutex);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_deepspeech_init (GstDeepSpeech * deepspeech)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM_CAST (deepspeech);
  deepspeech->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_segment_init (&trans->segment, GST_FORMAT_TIME);
  gst_pad_set_event_function (deepspeech->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_deepspeech_sink_event));
  gst_pad_set_chain_function (deepspeech->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_deepspeech_chain));
  GST_PAD_SET_PROXY_CAPS (deepspeech->sinkpad);
  gst_element_add_pad (GST_ELEMENT (deepspeech), deepspeech->sinkpad);

  deepspeech->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (deepspeech->srcpad);
  gst_element_add_pad (GST_ELEMENT (deepspeech), deepspeech->srcpad);

  deepspeech->speech_model_path = g_strdup (DEFAULT_SPEECH_MODEL);
  deepspeech->alphabet_path = g_strdup (DEFAULT_ALPHABET);
  deepspeech->language_model_path = g_strdup (DEFAULT_LANGUAGE_MODEL);
  deepspeech->trie_path = g_strdup (DEFAULT_TRIE);
  deepspeech->silence_threshold = DEFAULT_SILENCE_THRESHOLD;
  deepspeech->silence_length = DEFAULT_SILENCE_LENGTH;
  deepspeech->quiet_bufs = 0;
  gst_deepspeech_load_model (deepspeech);
  deepspeech->buf = gst_buffer_new();
  deepspeech->thread_pool = g_thread_pool_new((GFunc) run_model_async, (gpointer) deepspeech, -1, FALSE, NULL);
}

static void
gst_deepspeech_load_model (GstDeepSpeech * deepspeech)
{
  deepspeech->model = new Model(deepspeech->speech_model_path, N_CEP, N_CONTEXT, deepspeech->alphabet_path, BEAM_WIDTH);
  deepspeech->model->enableDecoderWithLM(deepspeech->alphabet_path, deepspeech->language_model_path, deepspeech->trie_path, LM_WEIGHT, WORD_COUNT_WEIGHT, VALID_WORD_COUNT_WEIGHT);
}

static void
gst_deepspeech_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDeepSpeech *deepspeech = GST_DEEPSPEECH (object);

  switch (prop_id) {
    case PROP_SPEECH_MODEL:
      deepspeech->speech_model_path = g_value_dup_string (value);
      gst_deepspeech_load_model (deepspeech);
      break;
    case PROP_ALPHABET:
      deepspeech->alphabet_path = g_value_dup_string (value);
      gst_deepspeech_load_model (deepspeech);
      break;
    case PROP_LANGUAGE_MODEL:
      deepspeech->language_model_path = g_value_dup_string (value);
      gst_deepspeech_load_model (deepspeech);
      break;
    case PROP_TRIE:
      deepspeech->trie_path = g_value_dup_string (value);
      gst_deepspeech_load_model (deepspeech);
      break;
    case PROP_SILENCE_THRESHOLD:
      deepspeech->silence_threshold = g_value_get_double (value);
      break;
    case PROP_SILENCE_LENGTH:
      deepspeech->silence_length = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_deepspeech_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDeepSpeech *deepspeech = GST_DEEPSPEECH (object);

  switch (prop_id) {
    case PROP_SPEECH_MODEL:
      g_value_set_string (value, deepspeech->speech_model_path);
      break;
    case PROP_ALPHABET:
      g_value_set_string (value, deepspeech->alphabet_path);
      break;
    case PROP_LANGUAGE_MODEL:
      g_value_set_string (value, deepspeech->language_model_path);
      break;
    case PROP_TRIE:
      g_value_set_string (value, deepspeech->trie_path);
      break;
    case PROP_SILENCE_THRESHOLD:
      g_value_set_double (value, deepspeech->silence_threshold);
      break;
    case PROP_SILENCE_LENGTH:
      g_value_set_int (value, deepspeech->silence_length);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstMessage *
gst_deepspeech_message_new (GstDeepSpeech * deepspeech, GstBuffer * buf, const char * text)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM_CAST (deepspeech);
  GstStructure *s;
  GstClockTime running_time, stream_time;

  running_time = gst_segment_to_running_time (&trans->segment, GST_FORMAT_TIME,
      GST_BUFFER_TIMESTAMP (buf));
  stream_time = gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME,
      GST_BUFFER_TIMESTAMP (buf));

  s = gst_structure_new ("deepspeech",
      "timestamp", G_TYPE_UINT64, GST_BUFFER_TIMESTAMP (buf),
      "stream-time", G_TYPE_UINT64, stream_time,
      "running-time", G_TYPE_UINT64, running_time,
      "duration", G_TYPE_UINT64, GST_BUFFER_DURATION (buf),
      "text", G_TYPE_STRING, text, NULL);

  return gst_message_new_element (GST_OBJECT (deepspeech), s);
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_deepspeech_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstDeepSpeech *deepspeech;
  gboolean ret;

  deepspeech = GST_DEEPSPEECH (parent);

  GST_LOG_OBJECT (deepspeech, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      GstCaps * caps;

      gst_event_parse_caps (event, &caps);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_EOS:
      if (gst_buffer_get_size(deepspeech->buf) > 0) {
        g_thread_pool_push(deepspeech->thread_pool, (gpointer) gst_buffer_copy_deep(deepspeech->buf), NULL);
      }
      g_thread_pool_free(deepspeech->thread_pool, FALSE, TRUE);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_deepspeech_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstDeepSpeech *deepspeech;
  deepspeech = GST_DEEPSPEECH (parent);

  GstMapInfo info;
  gint16 *in;
  register guint j;
  gdouble squaresum = 0.0;
  register gdouble square = 0.0;
  register gdouble peaksquare = 0.0;
  gdouble normalizer;
  gdouble ncs = 0.0;
  gdouble nps = 0.0;

  gst_buffer_map(buf, &info, GST_MAP_READ);
  in = (gint16 *)info.data;

  for (j = 0; j < info.size/2; j += 1) {
    square = ((gdouble) in[j]) * in[j];
    if (square > peaksquare) peaksquare = square;
    squaresum += square;
  }

  normalizer = (gdouble) (G_GINT64_CONSTANT(1) << 30);
  ncs = squaresum / normalizer;
  nps = peaksquare / normalizer;

  if (ncs > deepspeech->silence_threshold || gst_buffer_get_size(deepspeech->buf) > 0) {
    gst_buffer_ref(buf);
    deepspeech->buf = gst_buffer_append(deepspeech->buf, buf);
  }

  if (ncs < deepspeech->silence_threshold && gst_buffer_get_size(deepspeech->buf) > 0) {
    deepspeech->quiet_bufs++;
  } else {
    deepspeech->quiet_bufs = 0;
  }

  if (deepspeech->quiet_bufs > deepspeech->silence_length && gst_buffer_get_size(deepspeech->buf) > 0) {
      g_thread_pool_push(deepspeech->thread_pool, (gpointer) gst_buffer_copy_deep(deepspeech->buf), NULL);
      deepspeech->buf = gst_buffer_new();
      deepspeech->quiet_bufs = 0;
  }

  /* just push out the incoming buffer without touching it */
  return gst_pad_push (deepspeech->srcpad, buf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
deepspeech_init (GstPlugin * deepspeech)
{
  /* debug category for filtering log messages
   */
  GST_DEBUG_CATEGORY_INIT (gst_deepspeech_debug, "deepspeech",
      0, "Performs speech recognition using Mozilla's DeepSpeech model.");

  return gst_element_register (deepspeech, "deepspeech", GST_RANK_NONE,
      GST_TYPE_DEEPSPEECH);
}

/* gstreamer looks for this structure to register plugins
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    deepspeech,
    "Performs speech recognition using Mozilla's DeepSpeech model.",
    deepspeech_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
