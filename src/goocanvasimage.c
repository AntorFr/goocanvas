/*
 * GooCanvas. Copyright (C) 2005 Damon Chaplin.
 * Released under the GNU LGPL license. See COPYING for details.
 *
 * goocanvasimage.c - image item.
 */

/**
 * SECTION:goocanvasimage
 * @Title: GooCanvasImage
 * @Short_Description: an image item.
 *
 * GooCanvasImage represents an image item.
 *
 * <note><para>
 * It is usually necessary to set the "scale-to-fit" property to %TRUE to
 * scale the image to fit the given rectangle.
 * </para></note>
 *
 * It is a subclass of #GooCanvasItemSimple and so inherits all of the style
 * properties such as "operator" and "pointer-events".
 *
 * It also implements the #GooCanvasItem interface, so you can use the
 * #GooCanvasItem functions such as goo_canvas_item_raise() and
 * goo_canvas_item_rotate().
 *
 * To create a #GooCanvasImage use goo_canvas_image_new().
 *
 * To get or set the properties of an existing #GooCanvasImage, use
 * g_object_get() and g_object_set().
 */
#include <config.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include "goocanvasprivate.h"
#include "goocanvasimage.h"
#include "goocanvas.h"
#include "goocanvasutils.h"


typedef struct _GooCanvasImagePrivate GooCanvasImagePrivate;
struct _GooCanvasImagePrivate {
  gboolean scale_to_fit;
  gdouble alpha;
};

#define GOO_CANVAS_IMAGE_GET_PRIVATE(image)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((image), GOO_TYPE_CANVAS_IMAGE, GooCanvasImagePrivate))
#define GOO_CANVAS_IMAGE_MODEL_GET_PRIVATE(image)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((image), GOO_TYPE_CANVAS_IMAGE_MODEL, GooCanvasImagePrivate))


enum {
  PROP_0,

  PROP_PATTERN,
  PROP_X,
  PROP_Y,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_SCALE_TO_FIT,
  PROP_ALPHA,

  /* Convenience properties. */
  PROP_PIXBUF
};

static void goo_canvas_image_dispose      (GObject            *object);
static void goo_canvas_image_finalize     (GObject            *object);
static void canvas_item_interface_init    (GooCanvasItemIface *iface);
static void goo_canvas_image_get_property (GObject            *object,
					   guint               param_id,
					   GValue             *value,
					   GParamSpec         *pspec);
static void goo_canvas_image_set_property (GObject            *object,
					   guint               param_id,
					   const GValue       *value,
					   GParamSpec         *pspec);

G_DEFINE_TYPE_WITH_CODE (GooCanvasImage, goo_canvas_image,
			 GOO_TYPE_CANVAS_ITEM_SIMPLE,
			 G_IMPLEMENT_INTERFACE (GOO_TYPE_CANVAS_ITEM,
						canvas_item_interface_init))


static void
goo_canvas_image_install_common_properties (GObjectClass *gobject_class)
{
  g_object_class_install_property (gobject_class, PROP_PATTERN,
                                   g_param_spec_boxed ("pattern",
						       _("Pattern"),
						       _("The cairo pattern to paint"),
						       GOO_TYPE_CAIRO_PATTERN,
						       G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_X,
				   g_param_spec_double ("x",
							"X",
							_("The x coordinate of the image"),
							-G_MAXDOUBLE,
							G_MAXDOUBLE, 0.0,
							G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_Y,
				   g_param_spec_double ("y",
							"Y",
							_("The y coordinate of the image"),
							-G_MAXDOUBLE,
							G_MAXDOUBLE, 0.0,
							G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_WIDTH,
				   g_param_spec_double ("width",
							_("Width"),
							_("The width of the image"),
							0.0, G_MAXDOUBLE, 0.0,
							G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_HEIGHT,
				   g_param_spec_double ("height",
							_("Height"),
							_("The height of the image"),
							0.0, G_MAXDOUBLE, 0.0,
							G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SCALE_TO_FIT, 
                                   g_param_spec_boolean ("scale-to-fit",
							 _("Scale To Fit"),
							 _("If the image is scaled to fit the width and height settings"),
							 FALSE,
							 G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_ALPHA,
				   g_param_spec_double ("alpha",
							_("Alpha"),
							_("The opacity of the image, 0.0 is fully transparent, and 1.0 is opaque."),
							0.0, 1.0, 1.0,
							G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PIXBUF,
				   g_param_spec_object ("pixbuf",
							_("Pixbuf"),
							_("The GdkPixbuf to display"),
							GDK_TYPE_PIXBUF,
							G_PARAM_WRITABLE));
}


/* Gets the private data to use, from the model or from the item itself. */
static GooCanvasImagePrivate*
goo_canvas_image_get_private (gpointer object)
{
  GooCanvasItemSimple *simple;

  if (GOO_IS_CANVAS_IMAGE (object))
    {
      simple = (GooCanvasItemSimple*) object;
      if (simple->model)
	return GOO_CANVAS_IMAGE_MODEL_GET_PRIVATE (simple->model);
      else
	return GOO_CANVAS_IMAGE_GET_PRIVATE (object);
    }
  else
    {
      return GOO_CANVAS_IMAGE_MODEL_GET_PRIVATE (object);
    }
}


static void
goo_canvas_image_init (GooCanvasImage *image)
{
  GooCanvasImagePrivate *priv = GOO_CANVAS_IMAGE_GET_PRIVATE (image);

  image->image_data = g_slice_new0 (GooCanvasImageData);

  priv->alpha = 1.0;
}


/*
 * Convert the width and height to the canvas's units, from the pixbuf's size 
 * in pixels.
 */
static void
goo_canvas_image_convert_pixbuf_sizes (GooCanvasItem *item,
				       GooCanvasImageData *image_data)
{
  GooCanvas *canvas = goo_canvas_item_get_canvas (item);
  if (canvas)
    {
      goo_canvas_convert_units_from_pixels (canvas, 
					    &(image_data->width),
					    &(image_data->height));
    }
}


/**
 * goo_canvas_image_new:
 * @parent: the parent item, or %NULL. If a parent is specified, it will assume
 *  ownership of the item, and the item will automatically be freed when it is
 *  removed from the parent. Otherwise call g_object_unref() to free it.
 * @pixbuf: the #GdkPixbuf containing the image data, or %NULL.
 * @x: the x coordinate of the image.
 * @y: the y coordinate of the image.
 * @...: optional pairs of property names and values, and a terminating %NULL.
 * 
 * Creates a new image item.
 * 
 * <!--PARAMETERS-->
 *
 * Here's an example showing how to create an image at (100.0, 100.0), using
 * the given pixbuf at its natural width and height:
 *
 * <informalexample><programlisting>
 *  GooCanvasItem *image = goo_canvas_image_new (mygroup, pixbuf, 100.0, 100.0,
 *                                               NULL);
 * </programlisting></informalexample>
 *
 * This example creates an image scaled to a size of 200x200:
 *
 * <informalexample><programlisting>
 *  GooCanvasItem *image = goo_canvas_image_new (mygroup, pixbuf, 100.0, 100.0,
 *                                               "width", 200.0,
 *                                               "height", 200.0,
 *                                               "scale-to-fit", TRUE,
 *                                               NULL);
 * </programlisting></informalexample>
 *
 * Returns: a new image item.
 **/
GooCanvasItem*
goo_canvas_image_new (GooCanvasItem *parent,
		      GdkPixbuf     *pixbuf,
		      gdouble        x,
		      gdouble        y,
		      ...)
{
  GooCanvasItem *item;
  GooCanvasImage *image;
  GooCanvasImageData *image_data;
  const char *first_property;
  va_list var_args;

  item = g_object_new (GOO_TYPE_CANVAS_IMAGE, NULL);
  image = (GooCanvasImage*) item;

  image_data = image->image_data;
  image_data->x = x;
  image_data->y = y;

  if (pixbuf)
    {
      image_data->pattern = goo_canvas_cairo_pattern_from_pixbuf (pixbuf);
      image_data->width = gdk_pixbuf_get_width (pixbuf);
      image_data->height = gdk_pixbuf_get_height (pixbuf);

      goo_canvas_image_convert_pixbuf_sizes (parent, image_data);
    }

  va_start (var_args, y);
  first_property = va_arg (var_args, char*);
  if (first_property)
    g_object_set_valist ((GObject*) item, first_property, var_args);
  va_end (var_args);

  if (parent)
    {
      goo_canvas_item_add_child (parent, item, -1);
      g_object_unref (item);
    }

  return item;
}


static void
goo_canvas_image_dispose (GObject *object)
{
  GooCanvasItemSimple *simple = (GooCanvasItemSimple*) object;
  GooCanvasImage *image = (GooCanvasImage*) object;

  if (!simple->model)
    {
      cairo_pattern_destroy (image->image_data->pattern);
      image->image_data->pattern = NULL;
    }

  G_OBJECT_CLASS (goo_canvas_image_parent_class)->dispose (object);
}


static void
goo_canvas_image_finalize (GObject *object)
{
  GooCanvasItemSimple *simple = (GooCanvasItemSimple*) object;
  GooCanvasImage *image = (GooCanvasImage*) object;

  /* Free our data if we didn't have a model. (If we had a model it would
     have been reset in dispose() and simple_data will be NULL.) */
  if (simple->simple_data)
    g_slice_free (GooCanvasImageData, image->image_data);
  image->image_data = NULL;

  G_OBJECT_CLASS (goo_canvas_image_parent_class)->finalize (object);
}


static void
goo_canvas_image_get_common_property (GObject              *object,
				      GooCanvasImageData   *image_data,
				      guint                 prop_id,
				      GValue               *value,
				      GParamSpec           *pspec)
{
  GooCanvasImagePrivate *priv = goo_canvas_image_get_private (object);

  switch (prop_id)
    {
    case PROP_PATTERN:
      g_value_set_boxed (value, image_data->pattern);
      break;
    case PROP_X:
      g_value_set_double (value, image_data->x);
      break;
    case PROP_Y:
      g_value_set_double (value, image_data->y);
      break;
    case PROP_WIDTH:
      g_value_set_double (value, image_data->width);
      break;
    case PROP_HEIGHT:
      g_value_set_double (value, image_data->height);
      break;
    case PROP_SCALE_TO_FIT:
      g_value_set_boolean (value, priv->scale_to_fit);
      break;
    case PROP_ALPHA:
      g_value_set_double (value, priv->alpha);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}



static void
goo_canvas_image_get_property (GObject              *object,
			       guint                 prop_id,
			       GValue               *value,
			       GParamSpec           *pspec)
{
  GooCanvasImage *image = (GooCanvasImage*) object;

  goo_canvas_image_get_common_property (object, image->image_data, prop_id,
					value, pspec);
}


static gboolean
goo_canvas_image_set_common_property (GObject              *object,
				      GooCanvasImageData   *image_data,
				      guint                 prop_id,
				      const GValue         *value,
				      GParamSpec           *pspec)
{
  GooCanvasImagePrivate *priv = goo_canvas_image_get_private (object);
  GdkPixbuf *pixbuf;
  gboolean recompute_bounds = TRUE;

  switch (prop_id)
    {
    case PROP_PATTERN:
      cairo_pattern_destroy (image_data->pattern);
      image_data->pattern = g_value_get_boxed (value);
      cairo_pattern_reference (image_data->pattern);
      break;
    case PROP_X:
      image_data->x = g_value_get_double (value);
      break;
    case PROP_Y:
      image_data->y = g_value_get_double (value);
      break;
    case PROP_WIDTH:
      image_data->width = g_value_get_double (value);
      break;
    case PROP_HEIGHT:
      image_data->height = g_value_get_double (value);
      break;
    case PROP_SCALE_TO_FIT:
      priv->scale_to_fit = g_value_get_boolean (value);
      break;
    case PROP_PIXBUF:
      cairo_pattern_destroy (image_data->pattern);
      pixbuf = g_value_get_object (value);
      image_data->pattern = pixbuf ? goo_canvas_cairo_pattern_from_pixbuf (pixbuf) : NULL;
      image_data->width = pixbuf ? gdk_pixbuf_get_width (pixbuf) : 0;
      image_data->height = pixbuf ? gdk_pixbuf_get_height (pixbuf) : 0;

      if (GOO_IS_CANVAS_ITEM (object))
	goo_canvas_image_convert_pixbuf_sizes (GOO_CANVAS_ITEM (object),
					       image_data);

      break;
    case PROP_ALPHA:
      priv->alpha = g_value_get_double (value);
      recompute_bounds = FALSE;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  return recompute_bounds;
}


static void
goo_canvas_image_set_property (GObject              *object,
			       guint                 prop_id,
			       const GValue         *value,
			       GParamSpec           *pspec)
{
  GooCanvasItemSimple *simple = (GooCanvasItemSimple*) object;
  GooCanvasImage *image = (GooCanvasImage*) object;
  gboolean recompute_bounds;

  if (simple->model)
    {
      g_warning ("Can't set property of a canvas item with a model - set the model property instead");
      return;
    }

  recompute_bounds = goo_canvas_image_set_common_property (object,
							   image->image_data,
							   prop_id,
							   value, pspec);
  goo_canvas_item_simple_changed (simple, recompute_bounds);
}


static gboolean
goo_canvas_image_is_item_at (GooCanvasItemSimple *simple,
			     gdouble              x,
			     gdouble              y,
			     cairo_t             *cr,
			     gboolean             is_pointer_event)
{
  GooCanvasImage *image = (GooCanvasImage*) simple;
  GooCanvasImageData *image_data = image->image_data;

  if (x < image_data->x || (x > image_data->x + image_data->width)
      || y < image_data->y || (y > image_data->y + image_data->height))
    return FALSE;

  return TRUE;
}


static void
goo_canvas_image_update  (GooCanvasItemSimple  *simple,
			  cairo_t              *cr)
{
  GooCanvasImage *image = (GooCanvasImage*) simple;
  GooCanvasImageData *image_data = image->image_data;

  /* Compute the new bounds. */
  simple->bounds.x1 = image_data->x;
  simple->bounds.y1 = image_data->y;
  simple->bounds.x2 = image_data->x + image_data->width;
  simple->bounds.y2 = image_data->y + image_data->height;
}


static void
goo_canvas_image_paint (GooCanvasItemSimple   *simple,
			cairo_t               *cr,
			const GooCanvasBounds *bounds)
{
  GooCanvasImagePrivate *priv = goo_canvas_image_get_private (simple);
  GooCanvasImage *image = (GooCanvasImage*) simple;
  GooCanvasImageData *image_data = image->image_data;
  cairo_matrix_t matrix = { 1, 0, 0, 1, 0, 0 };
  cairo_surface_t *surface;
  gdouble width, height;

  if (!image_data->pattern)
    return;

#if 1
  if (priv->scale_to_fit)
    {
      if (cairo_pattern_get_surface (image_data->pattern, &surface)
	  == CAIRO_STATUS_SUCCESS
	  && cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE)
	{
	  width = cairo_image_surface_get_width (surface);
	  height = cairo_image_surface_get_height (surface);
	  cairo_matrix_scale (&matrix, width / image_data->width,
			      height / image_data->height);
	}
    }

  cairo_matrix_translate (&matrix, -image_data->x, -image_data->y);

  cairo_pattern_set_matrix (image_data->pattern, &matrix);
  goo_canvas_style_set_fill_options (simple->simple_data->style, cr);
  cairo_set_source (cr, image_data->pattern);
  cairo_rectangle (cr, image_data->x, image_data->y,
		   image_data->width, image_data->height);
  /* To have better performance, we don't use cairo_paint_with_alpha if
   * the image is not transparent at all. */
  if (priv->alpha != 1.0)
    cairo_paint_with_alpha (cr, priv->alpha);
  else
    cairo_fill (cr);
#else
  /* Using cairo_paint() used to be much slower than cairo_fill(), though
     they seem similar now. I'm not sure if it matters which we use. */
  cairo_matrix_init_translate (&matrix, -image_data->x, -image_data->y);
  cairo_pattern_set_matrix (image_data->pattern, &matrix);
  goo_canvas_style_set_fill_options (simple->simple_data->style, cr);
  cairo_set_source (cr, image_data->pattern);
  cairo_paint (cr);
#endif
}


static void
goo_canvas_image_set_model    (GooCanvasItem      *item,
			       GooCanvasItemModel *model)
{
  GooCanvasItemSimple *simple = (GooCanvasItemSimple*) item;
  GooCanvasImage *image = (GooCanvasImage*) item;
  GooCanvasImageModel *imodel = (GooCanvasImageModel*) model;

  /* If our data was allocated, free it. */
  if (!simple->model)
    {
      cairo_pattern_destroy (image->image_data->pattern);
      g_slice_free (GooCanvasImageData, image->image_data);
    }

  /* Now use the new model's data instead. */
  image->image_data = &imodel->image_data;

  /* Let the parent GooCanvasItemSimple code do the rest. */
  goo_canvas_item_simple_set_model (simple, model);
}


static void
canvas_item_interface_init (GooCanvasItemIface *iface)
{
  iface->set_model   = goo_canvas_image_set_model;
}


static void
goo_canvas_image_class_init (GooCanvasImageClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass*) klass;
  GooCanvasItemSimpleClass *simple_class = (GooCanvasItemSimpleClass*) klass;

  g_type_class_add_private (gobject_class, sizeof (GooCanvasImagePrivate));

  gobject_class->dispose  = goo_canvas_image_dispose;
  gobject_class->finalize = goo_canvas_image_finalize;

  gobject_class->get_property = goo_canvas_image_get_property;
  gobject_class->set_property = goo_canvas_image_set_property;

  simple_class->simple_update      = goo_canvas_image_update;
  simple_class->simple_paint       = goo_canvas_image_paint;
  simple_class->simple_is_item_at  = goo_canvas_image_is_item_at;

  goo_canvas_image_install_common_properties (gobject_class);
}



/**
 * SECTION:goocanvasimagemodel
 * @Title: GooCanvasImageModel
 * @Short_Description: a model for image items.
 *
 * GooCanvasImageModel represent a model for image items.
 *
 * <note><para>
 * It is usually necessary to set the "scale-to-fit" property to %TRUE to
 * scale the image to fit the given rectangle. When using units other than
 * %GTK_UNIT_PIXEL it is also necessary to set the "width" and "height"
 * properties to set the desired size.
 * </para></note>
 *
 * It is a subclass of #GooCanvasItemModelSimple and so inherits all of the
 * style properties such as "operator" and "pointer-events".
 *
 * It also implements the #GooCanvasItemModel interface, so you can use the
 * #GooCanvasItemModel functions such as goo_canvas_item_model_raise() and
 * goo_canvas_item_model_rotate().
 *
 * To create a #GooCanvasImageModel use goo_canvas_image_model_new().
 *
 * To get or set the properties of an existing #GooCanvasImageModel, use
 * g_object_get() and g_object_set().
 *
 * To respond to events such as mouse clicks on the image you must connect
 * to the signal handlers of the corresponding #GooCanvasImage objects.
 * (See goo_canvas_get_item() and #GooCanvas::item-created.)
 */

static void item_model_interface_init (GooCanvasItemModelIface *iface);
static void goo_canvas_image_model_dispose      (GObject            *object);
static void goo_canvas_image_model_get_property (GObject            *object,
						 guint               param_id,
						 GValue             *value,
						 GParamSpec         *pspec);
static void goo_canvas_image_model_set_property (GObject            *object,
						 guint               param_id,
						 const GValue       *value,
						 GParamSpec         *pspec);

G_DEFINE_TYPE_WITH_CODE (GooCanvasImageModel, goo_canvas_image_model,
			 GOO_TYPE_CANVAS_ITEM_MODEL_SIMPLE,
			 G_IMPLEMENT_INTERFACE (GOO_TYPE_CANVAS_ITEM_MODEL,
						item_model_interface_init))


static void
goo_canvas_image_model_class_init (GooCanvasImageModelClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass*) klass;

  g_type_class_add_private (gobject_class, sizeof (GooCanvasImagePrivate));

  gobject_class->dispose      = goo_canvas_image_model_dispose;

  gobject_class->get_property = goo_canvas_image_model_get_property;
  gobject_class->set_property = goo_canvas_image_model_set_property;

  goo_canvas_image_install_common_properties (gobject_class);
}


static void
goo_canvas_image_model_init (GooCanvasImageModel *emodel)
{
  GooCanvasImagePrivate *priv = GOO_CANVAS_IMAGE_MODEL_GET_PRIVATE (emodel);

  priv->alpha = 1.0;
}


/**
 * goo_canvas_image_model_new:
 * @parent: the parent model, or %NULL. If a parent is specified, it will
 *  assume ownership of the item, and the item will automatically be freed when
 *  it is removed from the parent. Otherwise call g_object_unref() to free it.
 * @pixbuf: the #GdkPixbuf containing the image data, or %NULL.
 * @x: the x coordinate of the image.
 * @y: the y coordinate of the image.
 * @...: optional pairs of property names and values, and a terminating %NULL.
 * 
 * Creates a new image model.
 * 
 * <!--PARAMETERS-->
 *
 * Here's an example showing how to create an image at (100.0, 100.0), using
 * the given pixbuf at its natural width and height:
 *
 * <informalexample><programlisting>
 *  GooCanvasItemModel *image = goo_canvas_image_model_new (mygroup, pixbuf, 100.0, 100.0,
 *                                                          NULL);
 * </programlisting></informalexample>
 *
 * This example creates an image scaled to a size of 200x200:
 *
 * <informalexample><programlisting>
 *  GooCanvasItemModel *image = goo_canvas_image_model_new (mygroup, pixbuf, 100.0, 100.0,
 *                                                          "width", 200.0,
 *                                                          "height", 200.0,
 *                                                          "scale-to-fit", TRUE,
 *                                                          NULL);
 * </programlisting></informalexample>
 *
 * Returns: a new image model.
 **/
GooCanvasItemModel*
goo_canvas_image_model_new (GooCanvasItemModel *parent,
			    GdkPixbuf          *pixbuf,
			    gdouble             x,
			    gdouble             y,
			    ...)
{
  GooCanvasItemModel *model;
  GooCanvasImageModel *imodel;
  GooCanvasImageData *image_data;
  const char *first_property;
  va_list var_args;

  model = g_object_new (GOO_TYPE_CANVAS_IMAGE_MODEL, NULL);
  imodel = (GooCanvasImageModel*) model;

  image_data = &imodel->image_data;
  image_data->x = x;
  image_data->y = y;

  if (pixbuf)
    {
      image_data->pattern = goo_canvas_cairo_pattern_from_pixbuf (pixbuf);
      image_data->width = gdk_pixbuf_get_width (pixbuf);
      image_data->height = gdk_pixbuf_get_height (pixbuf);

      /* This is not possible with a model, because we don't know the canvas
	 units being used. */
      /*goo_canvas_image_convert_pixbuf_sizes (item, image_data);*/
    }

  va_start (var_args, y);
  first_property = va_arg (var_args, char*);
  if (first_property)
    g_object_set_valist ((GObject*) model, first_property, var_args);
  va_end (var_args);

  if (parent)
    {
      goo_canvas_item_model_add_child (parent, model, -1);
      g_object_unref (model);
    }

  return model;
}


static void
goo_canvas_image_model_dispose (GObject *object)
{
  GooCanvasImageModel *imodel = (GooCanvasImageModel*) object;

  cairo_pattern_destroy (imodel->image_data.pattern);
  imodel->image_data.pattern = NULL;

  G_OBJECT_CLASS (goo_canvas_image_model_parent_class)->dispose (object);
}


static void
goo_canvas_image_model_get_property (GObject              *object,
				     guint                 prop_id,
				     GValue               *value,
				     GParamSpec           *pspec)
{
  GooCanvasImageModel *imodel = (GooCanvasImageModel*) object;

  goo_canvas_image_get_common_property (object, &imodel->image_data, prop_id,
					value, pspec);
}


static void
goo_canvas_image_model_set_property (GObject              *object,
				     guint                 prop_id,
				     const GValue         *value,
				     GParamSpec           *pspec)
{
  GooCanvasImageModel *imodel = (GooCanvasImageModel*) object;
  gboolean recompute_bounds;

  recompute_bounds = goo_canvas_image_set_common_property (object,
							   &imodel->image_data,
							   prop_id,
							   value, pspec);
  g_signal_emit_by_name (imodel, "changed", recompute_bounds);
}


static GooCanvasItem*
goo_canvas_image_model_create_item (GooCanvasItemModel *model,
				    GooCanvas          *canvas)
{
  GooCanvasItem *item;

  item = g_object_new (GOO_TYPE_CANVAS_IMAGE, NULL);
  goo_canvas_item_set_model (item, model);

  return item;
}


static void
item_model_interface_init (GooCanvasItemModelIface *iface)
{
  iface->create_item    = goo_canvas_image_model_create_item;
}

