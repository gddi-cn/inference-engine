//
// Created by cc on 2021/12/24.
//

#ifndef CPX_SRC_BASIC_MACRO_CONCEPTS_HPP_
#define CPX_SRC_BASIC_MACRO_CONCEPTS_HPP_

#define NO_COPYABLE(ClassId_)                       \
    ClassId_(const ClassId_ &) = delete;            \
    ClassId_ &operator=(const ClassId_ &) = delete;

#define NO_MOVABLE(ClassId_)                        \
    ClassId_(ClassId_ &&) = delete;                 \
    ClassId_ &operator=(ClassId_ &&) = delete;

#endif //CPX_SRC_BASIC_MACRO_CONCEPTS_HPP_
