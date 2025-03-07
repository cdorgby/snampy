#pragma once

/**
 * @file io.hpp
 *
 * @brief Main include file for the I/O & Coroutine library
 *
 *
 * The coroutines library provides a set of classes and functions to perform blocking I/O operations asynchronously
 * using using the C++20 coroutines feature.
 *
 */
#include <io/common.hpp>
#include <io/io_loop.hpp>
#include <io/waiter.hpp>
#include <io/awaitable.hpp>
#include <io/iotask.hpp>
#include <io/iobuf.hpp>
#include <io/mbox.hpp>

#include <io/impl/iobuf.hpp>
#include <io/impl/iobuf_pack.hpp>
#include <io/impl/io_loop.hpp>
#include <io/impl/epoll_poller.hpp>
#include <io/impl/waiter.hpp>