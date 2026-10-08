#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "Renderer/Graphics/IGraphicsDevice.h"

namespace TEN::Renderer::Native::DirectX11
{
    // Per-pass GPU profiler: one disjoint query per frame and a timestamp pair per scope.
    // Results are read without stalling, a few frames after submission.
    class DX11GpuTimer
    {
    private:
        static constexpr auto FRAME_LATENCY = 4;
        static constexpr auto SCOPE_COUNT   = (int)Graphics::GpuTimingScope::Count;

        struct FrameQueries
        {
            Microsoft::WRL::ComPtr<ID3D11Query> Disjoint           = nullptr;
            Microsoft::WRL::ComPtr<ID3D11Query> Start[SCOPE_COUNT] = {};
            Microsoft::WRL::ComPtr<ID3D11Query> End[SCOPE_COUNT]   = {};
            bool                                Used[SCOPE_COUNT]  = {};
            bool                                Pending            = false;
        };

        FrameQueries _frames[FRAME_LATENCY] = {};
        double       _results[SCOPE_COUNT]  = {};
        int          _currentFrame          = 0;
        bool         _isRecording           = false;
        bool         _hasFailed             = false;

        bool Initialize(ID3D11Device* device, FrameQueries& frame)
        {
            auto desc = D3D11_QUERY_DESC{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
            if (FAILED(device->CreateQuery(&desc, frame.Disjoint.GetAddressOf())))
                return false;

            desc.Query = D3D11_QUERY_TIMESTAMP;
            for (int i = 0; i < SCOPE_COUNT; i++)
            {
                if (FAILED(device->CreateQuery(&desc, frame.Start[i].GetAddressOf())) ||
                    FAILED(device->CreateQuery(&desc, frame.End[i].GetAddressOf())))
                {
                    return false;
                }
            }

            return true;
        }

        // Returns false while the GPU has not finished the frame yet.
        bool TryResolve(ID3D11DeviceContext* context, FrameQueries& frame)
        {
            auto disjoint = D3D11_QUERY_DATA_TIMESTAMP_DISJOINT{};
            if (context->GetData(frame.Disjoint.Get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
                return false;

            for (int i = 0; i < SCOPE_COUNT; i++)
            {
                if (!frame.Used[i])
                    continue;

                UINT64 start = 0;
                UINT64 end = 0;
                if (context->GetData(frame.Start[i].Get(), &start, sizeof(start), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
                    context->GetData(frame.End[i].Get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
                {
                    return false;
                }

                if (!disjoint.Disjoint && disjoint.Frequency != 0 && end >= start)
                    _results[i] = 1000.0 * (double)(end - start) / (double)disjoint.Frequency;
            }

            frame.Pending = false;
            return true;
        }

    public:
        DX11GpuTimer()
        {
            for (int i = 0; i < SCOPE_COUNT; i++)
                _results[i] = -1.0;
        }

        void BeginFrame(ID3D11Device* device, ID3D11DeviceContext* context)
        {
            _isRecording = false;
            if (_hasFailed)
                return;

            auto& frame = _frames[_currentFrame];
            if (frame.Pending && !TryResolve(context, frame))
                return;

            if (frame.Disjoint == nullptr && !Initialize(device, frame))
            {
                _hasFailed = true;
                TENLog("GPU timing disabled: timestamp query creation failed.", LogLevel::Warning);
                return;
            }

            for (int i = 0; i < SCOPE_COUNT; i++)
                frame.Used[i] = false;

            context->Begin(frame.Disjoint.Get());
            _isRecording = true;
        }

        void EndFrame(ID3D11DeviceContext* context)
        {
            if (!_isRecording)
                return;

            auto& frame = _frames[_currentFrame];
            context->End(frame.Disjoint.Get());
            frame.Pending = true;
            _currentFrame = (_currentFrame + 1) % FRAME_LATENCY;
            _isRecording = false;
        }

        void BeginScope(ID3D11DeviceContext* context, int scope)
        {
            if (!_isRecording)
                return;

            auto& frame = _frames[_currentFrame];
            context->End(frame.Start[scope].Get());
            frame.Used[scope] = true;
        }

        void EndScope(ID3D11DeviceContext* context, int scope)
        {
            if (!_isRecording || !_frames[_currentFrame].Used[scope])
                return;

            context->End(_frames[_currentFrame].End[scope].Get());
        }

        // Most recent resolved time of a scope in milliseconds; negative until available.
        double GetMilliseconds(int scope) const
        {
            return _results[scope];
        }
    };
}
