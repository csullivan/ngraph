// ----------------------------------------------------------------------------
// Copyright 2017 Nervana Systems Inc.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// ----------------------------------------------------------------------------

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>

#include "ngraph/codegen/execution_engine.hpp"

using namespace ngraph;

codegen::ExecutionEngine::ExecutionEngine()
    : m_execution_engine{nullptr}
{
}

codegen::ExecutionEngine::~ExecutionEngine()
{
    if (m_execution_engine)
    {
        m_execution_engine->runStaticConstructorsDestructors(true);
    }
}

bool codegen::ExecutionEngine::add_module(std::unique_ptr<llvm::Module>& module)
{
    if (module)
    {
        if (!m_execution_engine)
        {
            m_execution_engine.reset(llvm::EngineBuilder(move(module))
                                         .setEngineKind(llvm::EngineKind::JIT)
                                         .setOptLevel(llvm::CodeGenOpt::Aggressive)
                                         .setErrorStr(&m_jit_error)
                                         .create());

            if (!m_execution_engine)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}

void codegen::ExecutionEngine::finalize()
{
    if (m_execution_engine)
    {
        m_execution_engine->finalizeObject();
        m_execution_engine->runStaticConstructorsDestructors(false);
    }
    else
    {
        throw std::runtime_error(
            "Error in finalize: " +
            (m_jit_error.empty() ? "Could not create an execution engine" : m_jit_error));
    }
}