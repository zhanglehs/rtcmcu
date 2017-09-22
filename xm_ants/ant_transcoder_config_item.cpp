/**
 * AntTranscoder class
 * @author songshenyi <songshenyi@youku.com>
 * @since  2014/04/15
 */

#include <net/uri.h>
#include "global_var.h"
#include "ant_transcoder.h"


AntTranscoderConfigItem::AntTranscoderConfigItem(AntTranscoderConfigItem& item)
{
    this->config_name       = item.config_name;
    this->default_specified = item.default_specified;
    this->must_specified    = item.must_specified;
    this->specified         = item.specified;
    this->argument_name     = item.argument_name;
    this->need_value        = item.need_value;
    this->values            = item.values;
    this->category          = item.category;
}
