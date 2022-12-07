// *****************************************************************************
/*!
  \file      src/matrix_thread.hpp
  \copyright 2022-2023 J. Bakosi,
             All rights reserved. See the LICENSE file for details.
  \brief     Piac matrix client
*/
// *****************************************************************************
#pragma once

#include <string>

namespace piac {

extern bool g_matrix_connected;
extern bool g_matrix_shutdown;

//! Entry point to thread to communicate with a matrix server
void matrix_thread( const std::string& server,
                    const std::string& username,
                    const std::string& password,
                    const std::string& db_key );

} // piac::
