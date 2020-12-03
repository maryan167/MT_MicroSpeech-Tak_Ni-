/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/lite/micro/examples/hello_world/main_functions.h"

#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/examples/hello_world/constants.h"
#include "tensorflow/lite/micro/examples/hello_world/model.h"
#include "tensorflow/lite/micro/examples/hello_world/output_handler.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

// Globals, used for compatibility with Arduino-style sketches.
namespace {
tflite::ErrorReporter* error_reporter = nullptr;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;
int inference_count = 0;

// Create an area of memory to use for input, output, and intermediate arrays.
// Minimum arena size, at the time of writing. After allocating tensors
// you can retrieve this value by invoking interpreter.arena_used_bytes().
const int kModelArenaSize = 1024 * 50;
// Extra headroom for model + alignment + future interpreter changes.
const int kExtraArenaSize = 1024 + 1024*45;
const int kTensorArenaSize = kModelArenaSize + kExtraArenaSize;
//const int kTensorArenaSize = 1024;
uint8_t tensor_arena[kTensorArenaSize];
}  // namespace

void setup(uint16_t SliceCount, uint16_t SliceSize) {
  // Set up logging. Google style is to avoid globals or statics because of
  // lifetime uncertainty, but since this has a trivial destructor it's okay.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  model = tflite::GetModel(g_model);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    TF_LITE_REPORT_ERROR(error_reporter,
                         "Model provided is schema version %d not equal "
                         "to supported version %d.",
                         model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  // This pulls in all the operation implementations we need.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::AllOpsResolver resolver;

  // Build an interpreter to run the model with.
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors.
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed");
    return;
  }

  // Obtain pointers to the model's input and output tensors.
  input = interpreter->input(0);
  //  printf("\n\r model_input->dims->size \t= %d", model_input->dims->size);
  //  printf("\n\r model_input->dims->data[0] \t= %d", model_input->dims->data[0]);
  //  printf("\n\r model_input->dims->data[1] \t= %d\tFeatureSliceCount", model_input->dims->data[1]);
  //  printf("\n\r model_input->dims->data[2] \t= %d\tFeatureSliceSize\n\r", model_input->dims->data[2]);

  if ((input->dims->size != 4) || (input->dims->data[0] != 1) ||
      (input->dims->data[1] != SliceCount) ||
      (input->dims->data[2] != SliceSize)) {
    TF_LITE_REPORT_ERROR(error_reporter,
      "Bad input tensor parameters in model");
    return;
  }
  if (input->type != kTfLiteInt8)
    TF_LITE_REPORT_ERROR(error_reporter, "Bad input tensor type");

//  model_input->dims_signature-> = new float(SliceCount*SliceSize);
//  if (model_input_buffer == nullptr)
//	  TF_LITE_REPORT_ERROR(error_reporter,
//	                             "Bad model_input_buffer pointer");

  output = interpreter->output(0);

  // Keep track of how many inferences we have performed.
  inference_count = 0;
}

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"

/******************************************************************************
 * 	Function name: check
 **************************************
 *	Summary:
 * 		This function sets the data in the NN model, runs model and gets the output.
 *
 * 	Parameters:
 *		*data		-	array with input sound bits;
 *		frame_num	-	number FFT frames
 *		frame_size	-	number of sound bits for FFT.
 *		*answer		-	2D array indicates the probability: [0] - Tak, [1] - Ni.
 *
 * 	Return:
 *
 */
void check(const int16_t *data, uint16_t frame_num, uint16_t frame_size, int8_t *answer, int words_count){
  // Place our calculated x value in the model's input tensor
  //float correct_input_data1[1][250][64][1];
  int8_t temp;

  uint32_t data_pos = 0;
  uint32_t pos = frame_size/2;
  uint32_t t_pos;

  for (int i=0; i<frame_num; i++){
    t_pos = pos;
    for (int j=frame_size/2; j<frame_size; j++){
      temp = int8_t((float) data[t_pos]/256 - 128);
      //printf("%d ", temp);
      input->data.int8[data_pos] = temp;
      t_pos++;
      data_pos++;
    }
    pos += frame_size;
  }

  printf("\n\r");
  // Run inference, and report any error
  TfLiteStatus invoke_status = interpreter->Invoke();
  if (invoke_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "Invoke failed on x_val: %f\n");
    return;
  }

  // Read the predicted y value from the model's output tensor
  //printf("%d\n", output->data.int8[0]);
  //printf("%d\n", output->data.int8[1]);
  for (size_t i = 0; i < words_count; i++) {
	answer[i] = output->data.int8[i];
  }
	
  // Output the results. A custom HandleOutput function can be implemented
  // for each supported hardware target.
  // HandleOutput(error_reporter, x_val, y_val);
}
