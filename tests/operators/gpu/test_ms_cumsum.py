# Copyright 2021 Huawei Technologies Co., Ltd
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
import numpy as np
from tests.common.gen_random import random_gaussian
from akg.utils import kernel_exec as utils
from akg.utils.result_analysis import gpu_profiling
from akg.utils.format_transform import to_tvm_nd_array
from tests.common.tensorio import compare_tensor
from akg.composite import cumsum as cumsum_ir_builder

def gen_data(shape, dtype, axis, exclusive, reverse):
    support_list = {"float16": np.float16, "float32": np.float32}
    data = random_gaussian(shape, miu=1, sigma=0.1).astype(support_list[dtype])
    expect = np.cumsum(data, axis)
    output = np.full(shape, np.nan, dtype)
    return data, output, expect

def test_ms_cumsum(shape, dtype, axis=0, exclusive=False, reverse=False, poly_sch=False):
    def cumsum(input):
        op_attrs = {"axis": axis if isinstance(axis, list) else [axis], "exclusive": exclusive, "reverse": reverse}
        return cumsum_ir_builder([input,], op_attrs)
    if poly_sch:
        mod = utils.op_build_test(cumsum, [shape], [dtype], kernel_name="cumsum", attrs={"target": "cuda"})

    data, output, expect = gen_data(shape, dtype, axis, exclusive, reverse)
    output = utils.mod_launch(mod, (data, output), expect = expect)
    ret = compare_tensor(output, expect, rtol=5e-03, atol=1.e-8, equal_nan=True)
    print("Test {}".format("Pass" if ret else "Failed"))
    if not ret:
        print("Error cuda:==========================")
        print(mod.imported_modules[0].get_soure())
        raise AssertionError("Test fail")
    data, expect = to_tvm_nd_array([data, expect])
    gpu_profiling(mod, data, expect, 400)

