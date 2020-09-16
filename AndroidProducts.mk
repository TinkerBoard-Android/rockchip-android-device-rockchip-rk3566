#
# Copyright 2014 The Android Open-Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

PRODUCT_MAKEFILES := \
        $(LOCAL_DIR)/rk3566A_rgo/rk3566A_rgo.mk \
        $(LOCAL_DIR)/rk3566A_r/rk3566A_r.mk \

COMMON_LUNCH_CHOICES := \
    rk3566A_rgo-userdebug \
    rk3566A_rgo-user \
    rk3566A_r-userdebug \
    rk3566A_r-user \

