/******************************************************************************
 *
 *  Copyright 2015 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include "base/logging.h"

#include "device/include/esco_parameters.h"

static const enh_esco_params_t default_esco_parameters[ESCO_NUM_CODECS] = {
    // CVSD
    {.transmit_bandwidth = TXRX_64KBITS_RATE,
     .receive_bandwidth = TXRX_64KBITS_RATE,
     .transmit_coding_format = {.coding_format = ESCO_CODING_FORMAT_CVSD,
                                .company_id = 0x0000,
                                .vendor_specific_codec_id = 0x0000},
     .receive_coding_format = {.coding_format = ESCO_CODING_FORMAT_CVSD,
                               .company_id = 0x0000,
                               .vendor_specific_codec_id = 0x0000},
     .transmit_codec_frame_size = 60,
     .receive_codec_frame_size = 60,
     .input_bandwidth = INPUT_OUTPUT_64K_RATE,
     .output_bandwidth = INPUT_OUTPUT_64K_RATE,
     .input_coding_format = {.coding_format = ESCO_CODING_FORMAT_LINEAR,
                             .company_id = 0x0000,
                             .vendor_specific_codec_id = 0x0000},
     .output_coding_format = {.coding_format = ESCO_CODING_FORMAT_LINEAR,
                              .company_id = 0x0000,
                              .vendor_specific_codec_id = 0x0000},
     .input_coded_data_size = 16,
     .output_coded_data_size = 16,
     .input_pcm_data_format = ESCO_PCM_DATA_FORMAT_2_COMP,
     .output_pcm_data_format = ESCO_PCM_DATA_FORMAT_2_COMP,
     .input_pcm_payload_msb_position = 0,
     .output_pcm_payload_msb_position = 0,
     .input_data_path = ESCO_DATA_PATH_PCM,
     .output_data_path = ESCO_DATA_PATH_PCM,
     .input_transport_unit_size = 0x00,
     .output_transport_unit_size = 0x00,
#if (BTA_HFP_VERSION >= HFP_VERSION_1_7)
     .max_latency_ms = 12,
#else
     .max_latency_ms = 10,
#endif

     .packet_types =
         (ESCO_PKT_TYPES_MASK_HV1 | ESCO_PKT_TYPES_MASK_HV2 |
          ESCO_PKT_TYPES_MASK_HV3 | ESCO_PKT_TYPES_MASK_EV3 |
          ESCO_PKT_TYPES_MASK_EV4 | ESCO_PKT_TYPES_MASK_EV5 |
          ESCO_PKT_TYPES_MASK_NO_3_EV3 | ESCO_PKT_TYPES_MASK_NO_2_EV5 |
          ESCO_PKT_TYPES_MASK_NO_3_EV5),
#if (BTA_HFP_VERSION >= HFP_VERSION_1_7)
     .retransmission_effort = ESCO_RETRANSMISSION_QUALITY
#else
     .retransmission_effort = ESCO_RETRANSMISSION_POWER
#endif

    },
    // mSBC T1
    {.transmit_bandwidth = TXRX_64KBITS_RATE,
     .receive_bandwidth = TXRX_64KBITS_RATE,
     .transmit_coding_format = {.coding_format = ESCO_CODING_FORMAT_MSBC,
                                .company_id = 0x0000,
                                .vendor_specific_codec_id = 0x0000},
     .receive_coding_format = {.coding_format = ESCO_CODING_FORMAT_MSBC,
                               .company_id = 0x0000,
                               .vendor_specific_codec_id = 0x0000},
     .transmit_codec_frame_size = 60,
     .receive_codec_frame_size = 60,
     .input_bandwidth = INPUT_OUTPUT_128K_RATE,
     .output_bandwidth = INPUT_OUTPUT_128K_RATE,
     .input_coding_format = {.coding_format = ESCO_CODING_FORMAT_LINEAR,
                             .company_id = 0x0000,
                             .vendor_specific_codec_id = 0x0000},
     .output_coding_format = {.coding_format = ESCO_CODING_FORMAT_LINEAR,
                              .company_id = 0x0000,
                              .vendor_specific_codec_id = 0x0000},
     .input_coded_data_size = 16,
     .output_coded_data_size = 16,
     .input_pcm_data_format = ESCO_PCM_DATA_FORMAT_2_COMP,
     .output_pcm_data_format = ESCO_PCM_DATA_FORMAT_2_COMP,
     .input_pcm_payload_msb_position = 0,
     .output_pcm_payload_msb_position = 0,
     .input_data_path = ESCO_DATA_PATH_PCM,
     .output_data_path = ESCO_DATA_PATH_PCM,
     .input_transport_unit_size = 0x00,
     .output_transport_unit_size = 0x00,
     .max_latency_ms = 8,
     .packet_types =
         (ESCO_PKT_TYPES_MASK_EV3 | ESCO_PKT_TYPES_MASK_NO_3_EV3 |
          ESCO_PKT_TYPES_MASK_NO_2_EV5 | ESCO_PKT_TYPES_MASK_NO_3_EV5 |
          ESCO_PKT_TYPES_MASK_NO_2_EV3),
     .retransmission_effort = ESCO_RETRANSMISSION_QUALITY},
    // mSBC T2
    {.transmit_bandwidth = TXRX_64KBITS_RATE,
     .receive_bandwidth = TXRX_64KBITS_RATE,
     .transmit_coding_format = {.coding_format = ESCO_CODING_FORMAT_MSBC,
                                .company_id = 0x0000,
                                .vendor_specific_codec_id = 0x0000},
     .receive_coding_format = {.coding_format = ESCO_CODING_FORMAT_MSBC,
                               .company_id = 0x0000,
                               .vendor_specific_codec_id = 0x0000},
     .transmit_codec_frame_size = 60,
     .receive_codec_frame_size = 60,
     .input_bandwidth = INPUT_OUTPUT_128K_RATE,
     .output_bandwidth = INPUT_OUTPUT_128K_RATE,
     .input_coding_format = {.coding_format = ESCO_CODING_FORMAT_LINEAR,
                             .company_id = 0x0000,
                             .vendor_specific_codec_id = 0x0000},
     .output_coding_format = {.coding_format = ESCO_CODING_FORMAT_LINEAR,
                              .company_id = 0x0000,
                              .vendor_specific_codec_id = 0x0000},
     .input_coded_data_size = 16,
     .output_coded_data_size = 16,
     .input_pcm_data_format = ESCO_PCM_DATA_FORMAT_2_COMP,
     .output_pcm_data_format = ESCO_PCM_DATA_FORMAT_2_COMP,
     .input_pcm_payload_msb_position = 0,
     .output_pcm_payload_msb_position = 0,
     .input_data_path = ESCO_DATA_PATH_PCM,
     .output_data_path = ESCO_DATA_PATH_PCM,
     .input_transport_unit_size = 0x00,
     .output_transport_unit_size = 0x00,
     .max_latency_ms = 13,
     .packet_types =
         (ESCO_PKT_TYPES_MASK_EV3 | ESCO_PKT_TYPES_MASK_NO_3_EV3 |
          ESCO_PKT_TYPES_MASK_NO_2_EV5 | ESCO_PKT_TYPES_MASK_NO_3_EV5),
     .retransmission_effort = ESCO_RETRANSMISSION_QUALITY}};

enh_esco_params_t esco_parameters_for_codec(esco_codec_t codec) {
  CHECK(codec >= 0) << "codec index " << (int)codec << "< 0";
  CHECK(codec < ESCO_NUM_CODECS) << "codec index " << (int)codec << " > "
                                 << ESCO_NUM_CODECS;
  return default_esco_parameters[codec];
}
