/*
** Copyright (c) 2002-2016, Erik de Castro Lopo <erikd@mega-nerd.com>
** All rights reserved.
**
** This code is released under 2-clause BSD license. Please see the
** file at : https://github.com/erikd/libsamplerate/blob/master/COPYING
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "src_config.h"
#include "common.h"

#define SINC_MAGIC_MARKER   MAKE_MAGIC (' ', 's', 'i', 'n', 'c', ' ')

/*========================================================================================
*/

#define MAKE_INCREMENT_T(x)     ((increment_t) (x))

#define SHIFT_BITS              12
#define FP_ONE                  ((double) (((increment_t) 1) << SHIFT_BITS))
#define INV_FP_ONE              (1.0 / FP_ONE)

/*========================================================================================
*/

typedef int32_t increment_t ;
typedef float   coeff_t ;

#include "fastest_coeffs.h"
#include "mid_qual_coeffs.h"
#include "high_qual_coeffs.h"

typedef struct
{   int     sinc_magic_marker ;

    int     channels ;
    long    in_count, in_used ;
    long    out_count, out_gen ;

    int     coeff_half_len, index_inc ;

    double  src_ratio, input_index ;

    coeff_t const   *coeffs ;

    int     b_current, b_end, b_real_end, b_len ;

    /* Sure hope noone does more than 128 channels at once. */
    double left_calc [128], right_calc [128] ;

    /* C99 struct flexible array. */
    float   buffer [] ;
} SINC_FILTER ;

static int sinc_multichan_vari_process (SRC_PRIVATE *psrc, SRC_DATA *data) ;
static int sinc_hex_vari_process (SRC_PRIVATE *psrc, SRC_DATA *data) ;
static int sinc_quad_vari_process (SRC_PRIVATE *psrc, SRC_DATA *data) ;
static int sinc_stereo_vari_process (SRC_PRIVATE *psrc, SRC_DATA *data) ;
static int sinc_mono_vari_process (SRC_PRIVATE *psrc, SRC_DATA *data) ;

static int prepare_data (SINC_FILTER *filter, SRC_DATA *data, int half_filter_chan_len) WARN_UNUSED ;

static void sinc_reset (SRC_PRIVATE *psrc) ;
static int sinc_copy (SRC_PRIVATE *from, SRC_PRIVATE *to) ;

static inline increment_t
double_to_fp (double x)
{   return (increment_t) (lrint ((x) * FP_ONE)) ;
} /* double_to_fp */

static inline increment_t
int_to_fp (int x)
{   return (((increment_t) (x)) << SHIFT_BITS) ;
} /* int_to_fp */

static inline int
fp_to_int (increment_t x)
{   return (((x) >> SHIFT_BITS)) ;
} /* fp_to_int */

static inline increment_t
fp_fraction_part (increment_t x)
{   return ((x) & ((((increment_t) 1) << SHIFT_BITS) - 1)) ;
} /* fp_fraction_part */

static inline double
fp_to_double (increment_t x)
{   return fp_fraction_part (x) * INV_FP_ONE ;
} /* fp_to_double */


/*----------------------------------------------------------------------------------------
*/

const char*
sinc_get_name (int src_enum)
{
    switch (src_enum)
    {   case SRC_SINC_BEST_QUALITY :
            return "Best Sinc Interpolator" ;

        case SRC_SINC_MEDIUM_QUALITY :
            return "Medium Sinc Interpolator" ;

        case SRC_SINC_FASTEST :
            return "Fastest Sinc Interpolator" ;

        default: break ;
        } ;

    return NULL ;
} /* sinc_get_descrition */

const char*
sinc_get_description (int src_enum)
{
    switch (src_enum)
    {   case SRC_SINC_FASTEST :
            return "Band limited sinc interpolation, fastest, 97dB SNR, 80% BW." ;

        case SRC_SINC_MEDIUM_QUALITY :
            return "Band limited sinc interpolation, medium quality, 121dB SNR, 90% BW." ;

        case SRC_SINC_BEST_QUALITY :
            return "Band limited sinc interpolation, best quality, 144dB SNR, 96% BW." ;

        default :
            break ;
        } ;

    return NULL ;
} /* sinc_get_descrition */

int
sinc_set_converter (SRC_PRIVATE *psrc, int src_enum)
{   SINC_FILTER *filter, temp_filter ;
    increment_t count ;
    uint32_t bits ;

    /* Quick sanity check. */
    if (SHIFT_BITS >= sizeof (increment_t) * 8 - 1)
        return SRC_ERR_SHIFT_BITS ;

    if (psrc->private_data != NULL)
    {   free (psrc->private_data) ;
        psrc->private_data = NULL ;
        } ;

    memset (&temp_filter, 0, sizeof (temp_filter)) ;

    temp_filter.sinc_magic_marker = SINC_MAGIC_MARKER ;
    temp_filter.channels = psrc->channels ;

    if (psrc->channels > ARRAY_LEN (temp_filter.left_calc))
        return SRC_ERR_BAD_CHANNEL_COUNT ;
    else if (psrc->channels == 1)
    {   psrc->const_process = sinc_mono_vari_process ;
        psrc->vari_process = sinc_mono_vari_process ;
        }
    else
    if (psrc->channels == 2)
    {   psrc->const_process = sinc_stereo_vari_process ;
        psrc->vari_process = sinc_stereo_vari_process ;
        }
    else
    if (psrc->channels == 4)
    {   psrc->const_process = sinc_quad_vari_process ;
        psrc->vari_process = sinc_quad_vari_process ;
        }
    else
    if (psrc->channels == 6)
    {   psrc->const_process = sinc_hex_vari_process ;
        psrc->vari_process = sinc_hex_vari_process ;
        }
    else
    {   psrc->const_process = sinc_multichan_vari_process ;
        psrc->vari_process = sinc_multichan_vari_process ;
        } ;
    psrc->reset = sinc_reset ;
    psrc->copy = sinc_copy ;

    switch (src_enum)
    {   case SRC_SINC_FASTEST :
                temp_filter.coeffs = fastest_coeffs.coeffs ;
                temp_filter.coeff_half_len = ARRAY_LEN (fastest_coeffs.coeffs) - 2 ;
                temp_filter.index_inc = fastest_coeffs.increment ;
                break ;

        case SRC_SINC_MEDIUM_QUALITY :
                temp_filter.coeffs = slow_mid_qual_coeffs.coeffs ;
                temp_filter.coeff_half_len = ARRAY_LEN (slow_mid_qual_coeffs.coeffs) - 2 ;
                temp_filter.index_inc = slow_mid_qual_coeffs.increment ;
                break ;

        case SRC_SINC_BEST_QUALITY :
                temp_filter.coeffs = slow_high_qual_coeffs.coeffs ;
                temp_filter.coeff_half_len = ARRAY_LEN (slow_high_qual_coeffs.coeffs) - 2 ;
                temp_filter.index_inc = slow_high_qual_coeffs.increment ;
                break ;

        default :
                return SRC_ERR_BAD_CONVERTER ;
        } ;

    /*
    ** FIXME : This needs to be looked at more closely to see if there is
    ** a better way. Need to look at prepare_data () at the same time.
    */

    temp_filter.b_len = 3 * (int) lrint ((temp_filter.coeff_half_len + 2.0) / temp_filter.index_inc * SRC_MAX_RATIO + 1) ;
    temp_filter.b_len = MAX (temp_filter.b_len, 4096) ;
    temp_filter.b_len *= temp_filter.channels ;
    temp_filter.b_len += 1 ; // There is a <= check against samples_in_hand requiring a buffer bigger than the calculation above

    if ((filter = ZERO_ALLOC (SINC_FILTER, sizeof (SINC_FILTER) + sizeof (filter->buffer [0]) * (temp_filter.b_len + temp_filter.channels))) == NULL)
        return SRC_ERR_MALLOC_FAILED ;

    *filter = temp_filter ;
    memset (&temp_filter, 0xEE, sizeof (temp_filter)) ;

    psrc->private_data = filter ;

    sinc_reset (psrc) ;

    count = filter->coeff_half_len ;
    for (bits = 0 ; (MAKE_INCREMENT_T (1) << bits) < count ; bits++)
        count |= (MAKE_INCREMENT_T (1) << bits) ;

    if (bits + SHIFT_BITS - 1 >= (int) (sizeof (increment_t) * 8))
        return SRC_ERR_FILTER_LEN ;

    return SRC_ERR_NO_ERROR ;
} /* sinc_set_converter */

static void
sinc_reset (SRC_PRIVATE *psrc)
{   SINC_FILTER *filter ;

    filter = (SINC_FILTER*) psrc->private_data ;
    if (filter == NULL)
        return ;

    filter->b_current = filter->b_end = 0 ;
    filter->b_real_end = -1 ;

    filter->src_ratio = filter->input_index = 0.0 ;

    memset (filter->buffer, 0, filter->b_len * sizeof (filter->buffer [0])) ;

    /* Set this for a sanity check */
    memset (filter->buffer + filter->b_len, 0xAA, filter->channels * sizeof (filter->buffer [0])) ;
} /* sinc_reset */

static int
sinc_copy (SRC_PRIVATE *from, SRC_PRIVATE *to)
{
    if (from->private_data == NULL)
        return SRC_ERR_NO_PRIVATE ;

    SINC_FILTER *to_filter = NULL ;
    SINC_FILTER* from_filter = (SINC_FILTER*) from->private_data ;
    size_t private_length = sizeof (SINC_FILTER) + sizeof (from_filter->buffer [0]) * (from_filter->b_len + from_filter->channels) ;

    if ((to_filter = ZERO_ALLOC (SINC_FILTER, private_length)) == NULL)
        return SRC_ERR_MALLOC_FAILED ;

    memcpy (to_filter, from_filter, private_length) ;
    to->private_data = to_filter ;

    return SRC_ERR_NO_ERROR ;
} /* sinc_copy */

/*========================================================================================
**  Beware all ye who dare pass this point. There be dragons here.
*/

static inline double
calc_output_single (SINC_FILTER *filter, increment_t increment, increment_t start_filter_index)
{   double      fraction, left, right, icoeff ;
    increment_t filter_index, max_filter_index ;
    int         data_index, coeff_count, indx ;

    /* Convert input parameters into fixed point. */
    max_filter_index = int_to_fp (filter->coeff_half_len) ;

    /* First apply the left half of the filter. */
    filter_index = start_filter_index ;
    coeff_count = (max_filter_index - filter_index) / increment ;
    filter_index = filter_index + coeff_count * increment ;
    data_index = filter->b_current - coeff_count ;

    left = 0.0 ;
    do
    {   if (data_index >= 0) /* Avoid underflow access to filter->buffer. */
        {   fraction = fp_to_double (filter_index) ;
            indx = fp_to_int (filter_index) ;

            icoeff = filter->coeffs [indx] + fraction * (filter->coeffs [indx + 1] - filter->coeffs [indx]) ;

            left += icoeff * filter->buffer [data_index] ;
            }  ;

        filter_index -= increment ;
        data_index = data_index + 1 ;
        }
    while (filter_index >= MAKE_INCREMENT_T (0)) ;

    /* Now apply the right half of the filter. */
    filter_index = increment - start_filter_index ;
    coeff_count = (max_filter_index - filter_index) / increment ;
    filter_index = filter_index + coeff_count * increment ;
    data_index = filter->b_current + 1 + coeff_count ;

    right = 0.0 ;
    do
    {   fraction = fp_to_double (filter_index) ;
        indx = fp_to_int (filter_index) ;

        icoeff = filter->coeffs [indx] + fraction * (filter->coeffs [indx + 1] - filter->coeffs [indx]) ;

        right += icoeff * filter->buffer [data_index] ;

        filter_index -= increment ;
        data_index = data_index - 1 ;
        }
    while (filter_index > MAKE_INCREMENT_T (0)) ;

    return (left + right) ;
} /* calc_output_single */

static int
sinc_mono_vari_process (SRC_PRIVATE *psrc, SRC_DATA *data)
{   SINC_FILTER *filter ;
    double      input_index, src_ratio, count, float_increment, terminate, rem ;
    increment_t increment, start_filter_index ;
    int         half_filter_chan_len, samples_in_hand ;

    if (psrc->private_data == NULL)
        return SRC_ERR_NO_PRIVATE ;

    filter = (SINC_FILTER*) psrc->private_data ;

    /* If there is not a problem, this will be optimised out. */
    if (sizeof (filter->buffer [0]) != sizeof (data->data_in [0]))
        return SRC_ERR_SIZE_INCOMPATIBILITY ;

    filter->in_count = data->input_frames * filter->channels ;
    filter->out_count = data->output_frames * filter->channels ;
    filter->in_used = filter->out_gen = 0 ;

    src_ratio = psrc->last_ratio ;

    if (is_bad_src_ratio (src_ratio))
        return SRC_ERR_BAD_INTERNAL_STATE ;

    /* Check the sample rate ratio wrt the buffer len. */
    count = (filter->coeff_half_len + 2.0) / filter->index_inc ;
    if (MIN (psrc->last_ratio, data->src_ratio) < 1.0)
        count /= MIN (psrc->last_ratio, data->src_ratio) ;

    /* Maximum coefficientson either side of center point. */
    half_filter_chan_len = filter->channels * (int) (lrint (count) + 1) ;

    input_index = psrc->last_position ;

    rem = fmod_one (input_index) ;
    filter->b_current = (filter->b_current + filter->channels * lrint (input_index - rem)) % filter->b_len ;
    input_index = rem ;

    terminate = 1.0 / src_ratio + 1e-20 ;

    /* Main processing loop. */
    while (filter->out_gen < filter->out_count)
    {
        /* Need to reload buffer? */
        samples_in_hand = (filter->b_end - filter->b_current + filter->b_len) % filter->b_len ;

        if (samples_in_hand <= half_filter_chan_len)
        {   if ((psrc->error = prepare_data (filter, data, half_filter_chan_len)) != 0)
                return psrc->error ;

            samples_in_hand = (filter->b_end - filter->b_current + filter->b_len) % filter->b_len ;
            if (samples_in_hand <= half_filter_chan_len)
                break ;
            } ;

        /* This is the termination condition. */
        if (filter->b_real_end >= 0)
        {   if (filter->b_current + input_index + terminate > filter->b_real_end)
                break ;
            } ;

        if (filter->out_count > 0 && fabs (psrc->last_ratio - data->src_ratio) > 1e-10)
            src_ratio = psrc->last_ratio + filter->out_gen * (data->src_ratio - psrc->last_ratio) / filter->out_count ;

        float_increment = filter->index_inc * (src_ratio < 1.0 ? src_ratio : 1.0) ;
        increment = double_to_fp (float_increment) ;

        start_filter_index = double_to_fp (input_index * float_increment) ;

        data->data_out [filter->out_gen] = (float) ((float_increment / filter->index_inc) *
                                        calc_output_single (filter, increment, start_filter_index)) ;
        filter->out_gen ++ ;

        /* Figure out the next index. */
        input_index += 1.0 / src_ratio ;
        rem = fmod_one (input_index) ;

        filter->b_current = (filter->b_current + filter->channels * lrint (input_index - rem)) % filter->b_len ;
        input_index = rem ;
        } ;

    psrc->last_position = input_index ;

    /* Save current ratio rather then target ratio. */
    psrc->last_ratio = src_ratio ;

    data->input_frames_used = filter->in_used / filter->channels ;
    data->output_frames_gen = filter->out_gen / filter->channels ;

    return SRC_ERR_NO_ERROR ;
} /* sinc_mono_vari_process */

static inline void
calc_output_stereo (SINC_FILTER *filter, increment_t increment, increment_t start_filter_index, double scale, float * output)
{   double      fraction, left [2], right [2], icoeff ;
    increment_t filter_index, max_filter_index ;
    int         data_index, coeff_count, indx ;

    /* Convert input parameters into fixed point. */
    max_filter_index = int_to_fp (filter->coeff_half_len) ;

    /* First apply the left half of the filter. */
    filter_index = start_filter_index ;
    coeff_count = (max_filter_index - filter_index) / increment ;
    filter_index = filter_index + coeff_count * increment ;
    data_index = filter->b_current - filter->channels * coeff_count ;

    left [0] = left [1] = 0.0 ;
    do
    {   if (data_index >= 0) /* Avoid underflow access to filter->buffer. */
        {   fraction = fp_to_double (filter_index) ;
            indx = fp_to_int (filter_index) ;

            icoeff = filter->coeffs [indx] + fraction * (filter->coeffs [indx + 1] - filter->coeffs [indx]) ;

            left [0] += icoeff * filter->buffer [data_index] ;
            left [1] += icoeff * filter->buffer [data_index + 1] ;
            } ;

        filter_index -= increment ;
        data_index = data_index + 2 ;
        }
    while (filter_index >= MAKE_INCREMENT_T (0)) ;

    /* Now apply the right half of the filter. */
    filter_index = increment - start_filter_index ;
    coeff_count = (max_filter_index - filter_index) / increment ;
    filter_index = filter_index + coeff_count * increment ;
    data_index = filter->b_current + filter->channels * (1 + coeff_count) ;

    right [0] = right [1] = 0.0 ;
    do
    {   fraction = fp_to_double (filter_index) ;
        indx = fp_to_int (filter_index) ;

        icoeff = filter->coeffs [indx] + fraction * (filter->coeffs [indx + 1] - filter->coeffs [indx]) ;

        right [0] += icoeff * filter->buffer [data_index] ;
        right [1] += icoeff * filter->buffer [data_index + 1] ;

        filter_index -= increment ;
        data_index = data_index - 2 ;
        }
    while (filter_index > MAKE_INCREMENT_T (0)) ;

    output [0] = scale * (left [0] + right [0]) ;
    output [1] = scale * (left [1] + right [1]) ;
} /* calc_output_stereo */

static int
sinc_stereo_vari_process (SRC_PRIVATE *psrc, SRC_DATA *data)
{   SINC_FILTER *filter ;
    double      input_index, src_ratio, count, float_increment, terminate, rem ;
    increment_t increment, start_filter_index ;
    int         half_filter_chan_len, samples_in_hand ;

    if (psrc->private_data == NULL)
        return SRC_ERR_NO_PRIVATE ;

    filter = (SINC_FILTER*) psrc->private_data ;

    /* If there is not a problem, this will be optimised out. */
    if (sizeof (filter->buffer [0]) != sizeof (data->data_in [0]))
        return SRC_ERR_SIZE_INCOMPATIBILITY ;

    filter->in_count = data->input_frames * filter->channels ;
    filter->out_count = data->output_frames * filter->channels ;
    filter->in_used = filter->out_gen = 0 ;

    src_ratio = psrc->last_ratio ;

    if (is_bad_src_ratio (src_ratio))
        return SRC_ERR_BAD_INTERNAL_STATE ;

    /* Check the sample rate ratio wrt the buffer len. */
    count = (filter->coeff_half_len + 2.0) / filter->index_inc ;
    if (MIN (psrc->last_ratio, data->src_ratio) < 1.0)
        count /= MIN (psrc->last_ratio, data->src_ratio) ;

    /* Maximum coefficientson either side of center point. */
    half_filter_chan_len = filter->channels * (int) (lrint (count) + 1) ;

    input_index = psrc->last_position ;

    rem = fmod_one (input_index) ;
    filter->b_current = (filter->b_current + filter->channels * lrint (input_index - rem)) % filter->b_len ;
    input_index = rem ;

    terminate = 1.0 / src_ratio + 1e-20 ;

    /* Main processing loop. */
    while (filter->out_gen < filter->out_count)
    {
        /* Need to reload buffer? */
        samples_in_hand = (filter->b_end - filter->b_current + filter->b_len) % filter->b_len ;

        if (samples_in_hand <= half_filter_chan_len)
        {   if ((psrc->error = prepare_data (filter, data, half_filter_chan_len)) != 0)
                return psrc->error ;

            samples_in_hand = (filter->b_end - filter->b_current + filter->b_len) % filter->b_len ;
            if (samples_in_hand <= half_filter_chan_len)
                break ;
            } ;

        /* This is the termination condition. */
        if (filter->b_real_end >= 0)
        {   if (filter->b_current + input_index + terminate >= filter->b_real_end)
                break ;
            } ;

        if (filter->out_count > 0 && fabs (psrc->last_ratio - data->src_ratio) > 1e-10)
            src_ratio = psrc->last_ratio + filter->out_gen * (data->src_ratio - psrc->last_ratio) / filter->out_count ;

        float_increment = filter->index_inc * (src_ratio < 1.0 ? src_ratio : 1.0) ;
        increment = double_to_fp (float_increment) ;

        start_filter_index = double_to_fp (input_index * float_increment) ;

        calc_output_stereo (filter, increment, start_filter_index, float_increment / filter->index_inc, data->data_out + filter->out_gen) ;
        filter->out_gen += 2 ;

        /* Figure out the next index. */
        input_index += 1.0 / src_ratio ;
        rem = fmod_one (input_index) ;

        filter->b_current = (filter->b_current + filter->channels * lrint (input_index - rem)) % filter->b_len ;
        input_index = rem ;
        } ;

    psrc->last_position = input_index ;

    /* Save current ratio rather then target ratio. */
    psrc->last_ratio = src_ratio ;

    data->input_frames_used = filter->in_used / filter->channels ;
    data->output_frames_gen = filter->out_gen / filter->channels ;

    return SRC_ERR_NO_ERROR ;
} /* sinc_stereo_vari_process */

static inline void
calc_output_quad (SINC_FILTER *filter, increment_t increment, increment_t start_filter_index, double scale, float * output)
{   double      fraction, left [4], right [4], icoeff ;
    increment_t filter_index, max_filter_index ;
    int         data_index, coeff_count, indx ;

    /* Convert input parameters into fixed point. */
    max_filter_index = int_to_fp (filter->coeff_half_len) ;

    /* First apply the left half of the filter. */
    filter_index = start_filter_index ;
    coeff_count = (max_filter_index - filter_index) / increment ;
    filter_index = filter_index + coeff_count * increment ;
    data_index = filter->b_current - filter->channels * coeff_count ;

    left [0] = left [1] = left [2] = left [3] = 0.0 ;
    do
    {   if (data_index >= 0) /* Avoid underflow access to filter->buffer. */
        {   fraction = fp_to_double (filter_index) ;
            indx = fp_to_int (filter_index) ;

            icoeff = filter->coeffs [indx] + fraction * (filter->coeffs [indx + 1] - filter->coeffs [indx]) ;

            left [0] += icoeff * filter->buffer [data_index] ;
            left [1] += icoeff * filter->buffer [data_index + 1] ;
            left [2] += icoeff * filter->buffer [data_index + 2] ;
            left [3] += icoeff * filter->buffer [data_index + 3] ;
            } ;

        filter_index -= increment ;
        data_index = data_index + 4 ;
        }
    while (filter_index >= MAKE_INCREMENT_T (0)) ;

    /* Now apply the right half of the filter. */
    filter_index = increment - start_filter_index ;
    coeff_count = (max_filter_index - filter_index) / increment ;
    filter_index = filter_index + coeff_count * increment ;
    data_index = filter->b_current + filter->channels * (1 + coeff_count) ;

    right [0] = right [1] = right [2] = right [3] = 0.0 ;
    do
    {   fraction = fp_to_double (filter_index) ;
        indx = fp_to_int (filter_index) ;

        icoeff = filter->coeffs [indx] + fraction * (filter->coeffs [indx + 1] - filter->coeffs [indx]) ;

        right [0] += icoeff * filter->buffer [data_index] ;
        right [1] += icoeff * filter->buffer [data_index + 1] ;
        right [2] += icoeff * filter->buffer [data_index + 2] ;
        right [3] += icoeff * filter->buffer [data_index + 3] ;

        filter_index -= increment ;
        data_index = data_index - 4 ;
        }
    while (filter_index > MAKE_INCREMENT_T (0)) ;

    output [0] = scale * (left [0] + right [0]) ;
    output [1] = scale * (left [1] + right [1]) ;
    output [2] = scale * (left [2] + right [2]) ;
    output [3] = scale * (left [3] + right [3]) ;
} /* calc_output_quad */

static int
sinc_quad_vari_process (SRC_PRIVATE *psrc, SRC_DATA *data)
{   SINC_FILTER *filter ;
    double      input_index, src_ratio, count, float_increment, terminate, rem ;
    increment_t increment, start_filter_index ;
    int         half_filter_chan_len, samples_in_hand ;

    if (psrc->private_data == NULL)
        return SRC_ERR_NO_PRIVATE ;

    filter = (SINC_FILTER*) psrc->private_data ;

    /* If there is not a problem, this will be optimised out. */
    if (sizeof (filter->buffer [0]) != sizeof (data->data_in [0]))
        return SRC_ERR_SIZE_INCOMPATIBILITY ;

    filter->in_count = data->input_frames * filter->channels ;
    filter->out_count = data->output_frames * filter->channels ;
    filter->in_used = filter->out_gen = 0 ;

    src_ratio = psrc->last_ratio ;

    if (is_bad_src_ratio (src_ratio))
        return SRC_ERR_BAD_INTERNAL_STATE ;

    /* Check the sample rate ratio wrt the buffer len. */
    count = (filter->coeff_half_len + 2.0) / filter->index_inc ;
    if (MIN (psrc->last_ratio, data->src_ratio) < 1.0)
        count /= MIN (psrc->last_ratio, data->src_ratio) ;

    /* Maximum coefficientson either side of center point. */
    half_filter_chan_len = filter->channels * (int) (lrint (count) + 1) ;

    input_index = psrc->last_position ;

    rem = fmod_one (input_index) ;
    filter->b_current = (filter->b_current + filter->channels * lrint (input_index - rem)) % filter->b_len ;
    input_index = rem ;

    terminate = 1.0 / src_ratio + 1e-20 ;

    /* Main processing loop. */
    while (filter->out_gen < filter->out_count)
    {
        /* Need to reload buffer? */
        samples_in_hand = (filter->b_end - filter->b_current + filter->b_len) % filter->b_len ;

        if (samples_in_hand <= half_filter_chan_len)
        {   if ((psrc->error = prepare_data (filter, data, half_filter_chan_len)) != 0)
                return psrc->error ;

            samples_in_hand = (filter->b_end - filter->b_current + filter->b_len) % filter->b_len ;
            if (samples_in_hand <= half_filter_chan_len)
                break ;
            } ;

        /* This is the termination condition. */
        if (filter->b_real_end >= 0)
        {   if (filter->b_current + input_index + terminate >= filter->b_real_end)
                break ;
            } ;

        if (filter->out_count > 0 && fabs (psrc->last_ratio - data->src_ratio) > 1e-10)
            src_ratio = psrc->last_ratio + filter->out_gen * (data->src_ratio - psrc->last_ratio) / filter->out_count ;

        float_increment = filter->index_inc * (src_ratio < 1.0 ? src_ratio : 1.0) ;
        increment = double_to_fp (float_increment) ;

        start_filter_index = double_to_fp (input_index * float_increment) ;

        calc_output_quad (filter, increment, start_filter_index, float_increment / filter->index_inc, data->data_out + filter->out_gen) ;
        filter->out_gen += 4 ;

        /* Figure out the next index. */
        input_index += 1.0 / src_ratio ;
        rem = fmod_one (input_index) ;

        filter->b_current = (filter->b_current + filter->channels * lrint (input_index - rem)) % filter->b_len ;
        input_index = rem ;
        } ;

    psrc->last_position = input_index ;

    /* Save current ratio rather then target ratio. */
    psrc->last_ratio = src_ratio ;

    data->input_frames_used = filter->in_used / filter->channels ;
    data->output_frames_gen = filter->out_gen / filter->channels ;

    return SRC_ERR_NO_ERROR ;
} /* sinc_quad_vari_process */

static inline void
calc_output_hex (SINC_FILTER *filter, increment_t increment, increment_t start_filter_index, double scale, float * output)
{   double      fraction, left [6], right [6], icoeff ;
    increment_t filter_index, max_filter_index ;
    int         data_index, coeff_count, indx ;

    /* Convert input parameters into fixed point. */
    max_filter_index = int_to_fp (filter->coeff_half_len) ;

    /* First apply the left half of the filter. */
    filter_index = start_filter_index ;
    coeff_count = (max_filter_index - filter_index) / increment ;
    filter_index = filter_index + coeff_count * increment ;
    data_index = filter->b_current - filter->channels * coeff_count ;

    left [0] = left [1] = left [2] = left [3] = left [4] = left [5] = 0.0 ;
    do
    {   if (data_index >= 0) /* Avoid underflow access to filter->buffer. */
        {   fraction = fp_to_double (filter_index) ;
            indx = fp_to_int (filter_index) ;

            icoeff = filter->coeffs [indx] + fraction * (filter->coeffs [indx + 1] - filter->coeffs [indx]) ;

            left [0] += icoeff * filter->buffer [data_index] ;
            left [1] += icoeff * filter->buffer [data_index + 1] ;
            left [2] += icoeff * filter->buffer [data_index + 2] ;
            left [3] += icoeff * filter->buffer [data_index + 3] ;
            left [4] += icoeff * filter->buffer [data_index + 4] ;
            left [5] += icoeff * filter->buffer [data_index + 5] ;
            } ;

        filter_index -= increment ;
        data_index = data_index + 6 ;
        }
    while (filter_index >= MAKE_INCREMENT_T (0)) ;

    /* Now apply the right half of the filter. */
    filter_index = increment - start_filter_index ;
    coeff_count = (max_filter_index - filter_index) / increment ;
    filter_index = filter_index + coeff_count * increment ;
    data_index = filter->b_current + filter->channels * (1 + coeff_count) ;

    right [0] = right [1] = right [2] = right [3] = right [4] = right [5] = 0.0 ;
    do
    {   fraction = fp_to_double (filter_index) ;
        indx = fp_to_int (filter_index) ;

        icoeff = filter->coeffs [indx] + fraction * (filter->coeffs [indx + 1] - filter->coeffs [indx]) ;

        right [0] += icoeff * filter->buffer [data_index] ;
        right [1] += icoeff * filter->buffer [data_index + 1] ;
        right [2] += icoeff * filter->buffer [data_index + 2] ;
        right [3] += icoeff * filter->buffer [data_index + 3] ;
        right [4] += icoeff * filter->buffer [data_index + 4] ;
        right [5] += icoeff * filter->buffer [data_index + 5] ;

        filter_index -= increment ;
        data_index = data_index - 6 ;
        }
    while (filter_index > MAKE_INCREMENT_T (0)) ;

    output [0] = scale * (left [0] + right [0]) ;
    output [1] = scale * (left [1] + right [1]) ;
    output [2] = scale * (left [2] + right [2]) ;
    output [3] = scale * (left [3] + right [3]) ;
    output [4] = scale * (left [4] + right [4]) ;
    output [5] = scale * (left [5] + right [5]) ;
} /* calc_output_hex */

static int
sinc_hex_vari_process (SRC_PRIVATE *psrc, SRC_DATA *data)
{   SINC_FILTER *filter ;
    double      input_index, src_ratio, count, float_increment, terminate, rem ;
    increment_t increment, start_filter_index ;
    int         half_filter_chan_len, samples_in_hand ;

    if (psrc->private_data == NULL)
        return SRC_ERR_NO_PRIVATE ;

    filter = (SINC_FILTER*) psrc->private_data ;

    /* If there is not a problem, this will be optimised out. */
    if (sizeof (filter->buffer [0]) != sizeof (data->data_in [0]))
        return SRC_ERR_SIZE_INCOMPATIBILITY ;

    filter->in_count = data->input_frames * filter->channels ;
    filter->out_count = data->output_frames * filter->channels ;
    filter->in_used = filter->out_gen = 0 ;

    src_ratio = psrc->last_ratio ;

    if (is_bad_src_ratio (src_ratio))
        return SRC_ERR_BAD_INTERNAL_STATE ;

    /* Check the sample rate ratio wrt the buffer len. */
    count = (filter->coeff_half_len + 2.0) / filter->index_inc ;
    if (MIN (psrc->last_ratio, data->src_ratio) < 1.0)
        count /= MIN (psrc->last_ratio, data->src_ratio) ;

    /* Maximum coefficientson either side of center point. */
    half_filter_chan_len = filter->channels * (int) (lrint (count) + 1) ;

    input_index = psrc->last_position ;

    rem = fmod_one (input_index) ;
    filter->b_current = (filter->b_current + filter->channels * lrint (input_index - rem)) % filter->b_len ;
    input_index = rem ;

    terminate = 1.0 / src_ratio + 1e-20 ;

    /* Main processing loop. */
    while (filter->out_gen < filter->out_count)
    {
        /* Need to reload buffer? */
        samples_in_hand = (filter->b_end - filter->b_current + filter->b_len) % filter->b_len ;

        if (samples_in_hand <= half_filter_chan_len)
        {   if ((psrc->error = prepare_data (filter, data, half_filter_chan_len)) != 0)
                return psrc->error ;

            samples_in_hand = (filter->b_end - filter->b_current + filter->b_len) % filter->b_len ;
            if (samples_in_hand <= half_filter_chan_len)
                break ;
            } ;

        /* This is the termination condition. */
        if (filter->b_real_end >= 0)
        {   if (filter->b_current + input_index + terminate >= filter->b_real_end)
                break ;
            } ;

        if (filter->out_count > 0 && fabs (psrc->last_ratio - data->src_ratio) > 1e-10)
            src_ratio = psrc->last_ratio + filter->out_gen * (data->src_ratio - psrc->last_ratio) / filter->out_count ;

        float_increment = filter->index_inc * (src_ratio < 1.0 ? src_ratio : 1.0) ;
        increment = double_to_fp (float_increment) ;

        start_filter_index = double_to_fp (input_index * float_increment) ;

        calc_output_hex (filter, increment, start_filter_index, float_increment / filter->index_inc, data->data_out + filter->out_gen) ;
        filter->out_gen += 6 ;

        /* Figure out the next index. */
        input_index += 1.0 / src_ratio ;
        rem = fmod_one (input_index) ;

        filter->b_current = (filter->b_current + filter->channels * lrint (input_index - rem)) % filter->b_len ;
        input_index = rem ;
        } ;

    psrc->last_position = input_index ;

    /* Save current ratio rather then target ratio. */
    psrc->last_ratio = src_ratio ;

    data->input_frames_used = filter->in_used / filter->channels ;
    data->output_frames_gen = filter->out_gen / filter->channels ;

    return SRC_ERR_NO_ERROR ;
} /* sinc_hex_vari_process */

static inline void
calc_output_multi (SINC_FILTER *filter, increment_t increment, increment_t start_filter_index, int channels, double scale, float * output)
{   double      fraction, icoeff ;
    /* The following line is 1999 ISO Standard C. If your compiler complains, get a better compiler. */
    double      *left, *right ;
    increment_t filter_index, max_filter_index ;
    int         data_index, coeff_count, indx, ch ;

    left = filter->left_calc ;
    right = filter->right_calc ;

    /* Convert input parameters into fixed point. */
    max_filter_index = int_to_fp (filter->coeff_half_len) ;

    /* First apply the left half of the filter. */
    filter_index = start_filter_index ;
    coeff_count = (max_filter_index - filter_index) / increment ;
    filter_index = filter_index + coeff_count * increment ;
    data_index = filter->b_current - channels * coeff_count ;

    memset (left, 0, sizeof (left [0]) * channels) ;

    do
    {   fraction = fp_to_double (filter_index) ;
        indx = fp_to_int (filter_index) ;

        icoeff = filter->coeffs [indx] + fraction * (filter->coeffs [indx + 1] - filter->coeffs [indx]) ;

        if (data_index >= 0) /* Avoid underflow access to filter->buffer. */
        {   /*
            **  Duff's Device.
            **  See : http://en.wikipedia.org/wiki/Duff's_device
            */
            ch = channels ;
            do
            {   switch (ch % 8)
                {   default :
                        ch -- ;
                        left [ch] += icoeff * filter->buffer [data_index + ch] ;
                        /* Falls through. */
                    case 7 :
                        ch -- ;
                        left [ch] += icoeff * filter->buffer [data_index + ch] ;
                        /* Falls through. */
                    case 6 :
                        ch -- ;
                        left [ch] += icoeff * filter->buffer [data_index + ch] ;
                        /* Falls through. */
                    case 5 :
                        ch -- ;
                        left [ch] += icoeff * filter->buffer [data_index + ch] ;
                        /* Falls through. */
                    case 4 :
                        ch -- ;
                        left [ch] += icoeff * filter->buffer [data_index + ch] ;
                        /* Falls through. */
                    case 3 :
                        ch -- ;
                        left [ch] += icoeff * filter->buffer [data_index + ch] ;
                        /* Falls through. */
                    case 2 :
                        ch -- ;
                        left [ch] += icoeff * filter->buffer [data_index + ch] ;
                        /* Falls through. */
                    case 1 :
                        ch -- ;
                        left [ch] += icoeff * filter->buffer [data_index + ch] ;
                    } ;
                }
            while (ch > 0) ;
            } ;

        filter_index -= increment ;
        data_index = data_index + channels ;
        }
    while (filter_index >= MAKE_INCREMENT_T (0)) ;

    /* Now apply the right half of the filter. */
    filter_index = increment - start_filter_index ;
    coeff_count = (max_filter_index - filter_index) / increment ;
    filter_index = filter_index + coeff_count * increment ;
    data_index = filter->b_current + channels * (1 + coeff_count) ;

    memset (right, 0, sizeof (right [0]) * channels) ;
    do
    {   fraction = fp_to_double (filter_index) ;
        indx = fp_to_int (filter_index) ;

        icoeff = filter->coeffs [indx] + fraction * (filter->coeffs [indx + 1] - filter->coeffs [indx]) ;

        ch = channels ;
        do
        {
            switch (ch % 8)
            {   default :
                    ch -- ;
                    right [ch] += icoeff * filter->buffer [data_index + ch] ;
                    /* Falls through. */
                case 7 :
                    ch -- ;
                    right [ch] += icoeff * filter->buffer [data_index + ch] ;
                    /* Falls through. */
                case 6 :
                    ch -- ;
                    right [ch] += icoeff * filter->buffer [data_index + ch] ;
                    /* Falls through. */
                case 5 :
                    ch -- ;
                    right [ch] += icoeff * filter->buffer [data_index + ch] ;
                    /* Falls through. */
                case 4 :
                    ch -- ;
                    right [ch] += icoeff * filter->buffer [data_index + ch] ;
                    /* Falls through. */
                case 3 :
                    ch -- ;
                    right [ch] += icoeff * filter->buffer [data_index + ch] ;
                    /* Falls through. */
                case 2 :
                    ch -- ;
                    right [ch] += icoeff * filter->buffer [data_index + ch] ;
                    /* Falls through. */
                case 1 :
                    ch -- ;
                    right [ch] += icoeff * filter->buffer [data_index + ch] ;
                } ;
            }
        while (ch > 0) ;

        filter_index -= increment ;
        data_index = data_index - channels ;
        }
    while (filter_index > MAKE_INCREMENT_T (0)) ;

    ch = channels ;
    do
    {
        switch (ch % 8)
        {   default :
                ch -- ;
                output [ch] = scale * (left [ch] + right [ch]) ;
                /* Falls through. */
            case 7 :
                ch -- ;
                output [ch] = scale * (left [ch] + right [ch]) ;
                /* Falls through. */
            case 6 :
                ch -- ;
                output [ch] = scale * (left [ch] + right [ch]) ;
                /* Falls through. */
            case 5 :
                ch -- ;
                output [ch] = scale * (left [ch] + right [ch]) ;
                /* Falls through. */
            case 4 :
                ch -- ;
                output [ch] = scale * (left [ch] + right [ch]) ;
                /* Falls through. */
            case 3 :
                ch -- ;
                output [ch] = scale * (left [ch] + right [ch]) ;
                /* Falls through. */
            case 2 :
                ch -- ;
                output [ch] = scale * (left [ch] + right [ch]) ;
                /* Falls through. */
            case 1 :
                ch -- ;
                output [ch] = scale * (left [ch] + right [ch]) ;
            } ;
        }
    while (ch > 0) ;

    return ;
} /* calc_output_multi */

static int
sinc_multichan_vari_process (SRC_PRIVATE *psrc, SRC_DATA *data)
{   SINC_FILTER *filter ;
    double      input_index, src_ratio, count, float_increment, terminate, rem ;
    increment_t increment, start_filter_index ;
    int         half_filter_chan_len, samples_in_hand ;

    if (psrc->private_data == NULL)
        return SRC_ERR_NO_PRIVATE ;

    filter = (SINC_FILTER*) psrc->private_data ;

    /* If there is not a problem, this will be optimised out. */
    if (sizeof (filter->buffer [0]) != sizeof (data->data_in [0]))
        return SRC_ERR_SIZE_INCOMPATIBILITY ;

    filter->in_count = data->input_frames * filter->channels ;
    filter->out_count = data->output_frames * filter->channels ;
    filter->in_used = filter->out_gen = 0 ;

    src_ratio = psrc->last_ratio ;

    if (is_bad_src_ratio (src_ratio))
        return SRC_ERR_BAD_INTERNAL_STATE ;

    /* Check the sample rate ratio wrt the buffer len. */
    count = (filter->coeff_half_len + 2.0) / filter->index_inc ;
    if (MIN (psrc->last_ratio, data->src_ratio) < 1.0)
        count /= MIN (psrc->last_ratio, data->src_ratio) ;

    /* Maximum coefficientson either side of center point. */
    half_filter_chan_len = filter->channels * (int) (lrint (count) + 1) ;

    input_index = psrc->last_position ;

    rem = fmod_one (input_index) ;
    filter->b_current = (filter->b_current + filter->channels * lrint (input_index - rem)) % filter->b_len ;
    input_index = rem ;

    terminate = 1.0 / src_ratio + 1e-20 ;

    /* Main processing loop. */
    while (filter->out_gen < filter->out_count)
    {
        /* Need to reload buffer? */
        samples_in_hand = (filter->b_end - filter->b_current + filter->b_len) % filter->b_len ;

        if (samples_in_hand <= half_filter_chan_len)
        {   if ((psrc->error = prepare_data (filter, data, half_filter_chan_len)) != 0)
                return psrc->error ;

            samples_in_hand = (filter->b_end - filter->b_current + filter->b_len) % filter->b_len ;
            if (samples_in_hand <= half_filter_chan_len)
                break ;
            } ;

        /* This is the termination condition. */
        if (filter->b_real_end >= 0)
        {   if (filter->b_current + input_index + terminate >= filter->b_real_end)
                break ;
            } ;

        if (filter->out_count > 0 && fabs (psrc->last_ratio - data->src_ratio) > 1e-10)
            src_ratio = psrc->last_ratio + filter->out_gen * (data->src_ratio - psrc->last_ratio) / filter->out_count ;

        float_increment = filter->index_inc * (src_ratio < 1.0 ? src_ratio : 1.0) ;
        increment = double_to_fp (float_increment) ;

        start_filter_index = double_to_fp (input_index * float_increment) ;

        calc_output_multi (filter, increment, start_filter_index, filter->channels, float_increment / filter->index_inc, data->data_out + filter->out_gen) ;
        filter->out_gen += psrc->channels ;

        /* Figure out the next index. */
        input_index += 1.0 / src_ratio ;
        rem = fmod_one (input_index) ;

        filter->b_current = (filter->b_current + filter->channels * lrint (input_index - rem)) % filter->b_len ;
        input_index = rem ;
        } ;

    psrc->last_position = input_index ;

    /* Save current ratio rather then target ratio. */
    psrc->last_ratio = src_ratio ;

    data->input_frames_used = filter->in_used / filter->channels ;
    data->output_frames_gen = filter->out_gen / filter->channels ;

    return SRC_ERR_NO_ERROR ;
} /* sinc_multichan_vari_process */

/*----------------------------------------------------------------------------------------
*/

static int
prepare_data (SINC_FILTER *filter, SRC_DATA *data, int half_filter_chan_len)
{   int len = 0 ;

    if (filter->b_real_end >= 0)
        return 0 ;  /* Should be terminating. Just return. */

    if (data->data_in == NULL)
        return 0 ;

    if (filter->b_current == 0)
    {   /* Initial state. Set up zeros at the start of the buffer and
        ** then load new data after that.
        */
        len = filter->b_len - 2 * half_filter_chan_len ;

        filter->b_current = filter->b_end = half_filter_chan_len ;
        }
    else if (filter->b_end + half_filter_chan_len + filter->channels < filter->b_len)
    {   /*  Load data at current end position. */
        len = MAX (filter->b_len - filter->b_current - half_filter_chan_len, 0) ;
        }
    else
    {   /* Move data at end of buffer back to the start of the buffer. */
        len = filter->b_end - filter->b_current ;
        memmove (filter->buffer, filter->buffer + filter->b_current - half_filter_chan_len,
                        (half_filter_chan_len + len) * sizeof (filter->buffer [0])) ;

        filter->b_current = half_filter_chan_len ;
        filter->b_end = filter->b_current + len ;

        /* Now load data at current end of buffer. */
        len = MAX (filter->b_len - filter->b_current - half_filter_chan_len, 0) ;
        } ;

    len = MIN ((int) (filter->in_count - filter->in_used), len) ;
    len -= (len % filter->channels) ;

    if (len < 0 || filter->b_end + len > filter->b_len)
        return SRC_ERR_SINC_PREPARE_DATA_BAD_LEN ;

    memcpy (filter->buffer + filter->b_end, data->data_in + filter->in_used,
                        len * sizeof (filter->buffer [0])) ;

    filter->b_end += len ;
    filter->in_used += len ;

    if (filter->in_used == filter->in_count &&
            filter->b_end - filter->b_current < 2 * half_filter_chan_len && data->end_of_input)
    {   /* Handle the case where all data in the current buffer has been
        ** consumed and this is the last buffer.
        */

        if (filter->b_len - filter->b_end < half_filter_chan_len + 5)
        {   /* If necessary, move data down to the start of the buffer. */
            len = filter->b_end - filter->b_current ;
            memmove (filter->buffer, filter->buffer + filter->b_current - half_filter_chan_len,
                            (half_filter_chan_len + len) * sizeof (filter->buffer [0])) ;

            filter->b_current = half_filter_chan_len ;
            filter->b_end = filter->b_current + len ;
            } ;

        filter->b_real_end = filter->b_end ;
        len = half_filter_chan_len + 5 ;

        if (len < 0 || filter->b_end + len > filter->b_len)
            len = filter->b_len - filter->b_end ;

        memset (filter->buffer + filter->b_end, 0, len * sizeof (filter->buffer [0])) ;
        filter->b_end += len ;
        } ;

    return 0 ;
} /* prepare_data */
