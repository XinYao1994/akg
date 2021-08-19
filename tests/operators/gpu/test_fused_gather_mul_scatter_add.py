# Copyright 2020-2021 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License

from tests.common.base import get_rtol_atol
from tests.common.tensorio import compare_tensor
from akg.utils import kernel_exec as utils
from akg.ops.array_gpu import fused_gather_mul_scatter_add
from akg.utils.result_analysis import gpu_profiling
from akg.utils.format_transform import to_tvm_nd_array
from tests.common.gen_random import random_gaussian
import numpy as np
from copy import deepcopy

def gen_data(shape1, shape2, shape3, shape4, dtype1, dtype2, axis):
    # gather
    input1 = random_gaussian(shape1).astype(dtype1)
    input2 = np.random.randint(low=0, high=shape1[axis], size=shape2).astype(dtype2)
    gather_out = np.take(input1, input2, axis=axis)

    # mul
    input3 = random_gaussian(shape3).astype(dtype1)
    mul_out = np.multiply(gather_out, input3)

    # scatter_add
    params = np.zeros(shape1, dtype1)
    #params = random_gaussian(shape1).astype(dtype1)
    indices = np.zeros(shape4, dtype2)
    original_shape = indices.shape
    indices = indices.reshape(-1, indices.shape[-1])
    for i in range(indices.shape[0]):
        for j in range(indices.shape[1]):
            indices[i][j] = np.random.randint(shape1[j], size=())
    
    indices = indices.reshape(original_shape)
    expect = deepcopy(params)
    np.add.at(expect, tuple(indices.T.tolist()), mul_out)
    indices = indices.reshape(shape4)
    return input1, input2, input3, indices, expect

def test_fused_gather_mul_scatter_add(input1_shape, input2_shape, input3_shape, input4_shape, data_dtype, indices_type, axis, poly_sch=False, attrs=None):
    op_attrs = [axis]
    default_attrs = {"target": "cuda"}
    if attrs:
        default_attrs.update(attrs)

    if poly_sch:
        mod = utils.op_build_test(fused_gather_mul_scatter_add.fused_gather_mul_scatter_add,
                                                               [input1_shape, input2_shape, input3_shape, input4_shape], [data_dtype, indices_type, data_dtype, indices_type], op_attrs=op_attrs,
                                                               attrs=default_attrs, kernel_name="fused_gather_mul_scatter_add", )

    # gen data
    input1, input2, input3, input4, expect = gen_data(input1_shape, input2_shape, input3_shape, input4_shape, data_dtype, indices_type, axis)
    output_shape = expect.shape

    if len(expect.shape) == 0:
        output_shape = (1, )
    #output = np.full(output_shape, np.nan, expect.dtype)
    output = np.zeros(output_shape, expect.dtype)
    output = utils.mod_launch(mod, (input1, input2, input3, input4, output), expect = expect)

    atol, rtol = get_rtol_atol("fused_gather_mul_scatter_add", data_dtype)
    res = compare_tensor(output, expect, rtol=rtol, atol=atol)
    print("Test {}".format("Pass" if res else "Failed"))
    if not res:
        print("Error cuda:========================")
        print(mod.imported_modules[0].get_source())
        raise AssertionError("Test fail")

    input1, input2, input3, input4, output, expect = to_tvm_nd_array(
        [input1, input2, input3, input4, output, expect])
    gpu_profiling(mod, input1, input2, input3, input4, output, expect, repeat_time=400)
