#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "Specific/trutils.h"

namespace TEN::Renderer::Native::DirectX11
{
    class DX11GpuTimer
    {
    private:
        static constexpr auto QUERY_COUNT = 6;
        static constexpr auto REPORT_SAMPLES = 120;

        struct QuerySet
        {
            Microsoft::WRL::ComPtr<ID3D11Query> Disjoint;
            Microsoft::WRL::ComPtr<ID3D11Query> Start;
            Microsoft::WRL::ComPtr<ID3D11Query> End;
            bool Pending = false;
        };

        QuerySet _queries[QUERY_COUNT];
        int _nextQuery = 0;
        int _samples = 0;
        int _skipped = 0;
        int _disjoint = 0;
        double _total = 0.0;
        double _minimum = 0.0;
        double _maximum = 0.0;
        bool _recording = false;
        bool _failed = false;

        bool Initialize(ID3D11Device* device, QuerySet& query)
        {
            auto desc = D3D11_QUERY_DESC{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
            auto result = device->CreateQuery(&desc, query.Disjoint.GetAddressOf());
            desc.Query = D3D11_QUERY_TIMESTAMP;
            if (SUCCEEDED(result))
                result = device->CreateQuery(&desc, query.Start.GetAddressOf());
            if (SUCCEEDED(result))
                result = device->CreateQuery(&desc, query.End.GetAddressOf());
            if (SUCCEEDED(result))
                return true;

            _failed = true;
            TENLog("GPU timing disabled: timestamp query creation failed.", LogLevel::Warning);
            return false;
        }

        bool TryRead(ID3D11DeviceContext* context, QuerySet& query, const char* name)
        {
            auto disjoint = D3D11_QUERY_DATA_TIMESTAMP_DISJOINT{};
            UINT64 start = 0;
            UINT64 end = 0;
            if (context->GetData(query.Disjoint.Get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
                context->GetData(query.Start.Get(), &start, sizeof(start), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
                context->GetData(query.End.Get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
            {
                return false;
            }

            query.Pending = false;
            if (disjoint.Disjoint || disjoint.Frequency == 0 || end < start)
                _disjoint++;
            else
                Accumulate(1000.0 * (double)(end - start) / (double)disjoint.Frequency, name);
            return true;
        }

        void Accumulate(double milliseconds, const char* name)
        {
            _minimum = (_samples == 0) ? milliseconds : std::min(_minimum, milliseconds);
            _maximum = std::max(_maximum, milliseconds);
            _total += milliseconds;
            if (++_samples < REPORT_SAMPLES)
                return;

            TENLog(fmt::format("GPU {}: avg {:.3f} ms, min {:.3f}, max {:.3f}; {} samples, {} skipped, {} disjoint.",
                name, _total / _samples, _minimum, _maximum, _samples, _skipped, _disjoint), LogLevel::Info);
            _samples = _skipped = _disjoint = 0;
            _total = _minimum = _maximum = 0.0;
        }

    public:
        void Begin(ID3D11Device* device, ID3D11DeviceContext* context, const char* name)
        {
            _recording = false;
            if (_failed)
                return;

            auto& query = _queries[_nextQuery];
            if (query.Pending && !TryRead(context, query, name))
            {
                _skipped++;
                return;
            }
            if (!query.Disjoint && !Initialize(device, query))
                return;

            context->Begin(query.Disjoint.Get());
            context->End(query.Start.Get());
            _recording = true;
        }

        void End(ID3D11DeviceContext* context)
        {
            if (!_recording)
                return;

            auto& query = _queries[_nextQuery];
            context->End(query.End.Get());
            context->End(query.Disjoint.Get());
            query.Pending = true;
            _nextQuery = (_nextQuery + 1) % QUERY_COUNT;
            _recording = false;
        }
    };
}
