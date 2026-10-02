#pragma once
#include <libslic3r/Nonplanar/JobArtifact.hpp>
#include <libslic3r/Nonplanar/JobNative.hpp>
#include <nlohmann/json.hpp>
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>

namespace Slic3r::nptop::test {
// Optional diagnostic fixtures, outside every production export route.
inline void save_job_report(const boost::filesystem::path &path,const GuardedCandidateReportResult &result)
{
    REQUIRE(result.snapshot);const auto &report=*result.snapshot;const auto &binding=*report.binding;
    const auto &candidate=*binding.candidate;const auto &ledger=*candidate.plan->planned->material->ledger;
    nlohmann::json record={{"canonical",report.canonical_json},{"sha256",report.sha256},{"manifest",binding.manifest_json},
        {"manifest_sha256",binding.manifest_sha256},{"candidate_bytes",candidate.bytes},{"candidate_sha256",candidate.sha256},
        {"records",report.rates ? report.rates->moves.size() : 0},{"evaluations",result.evaluations},
        {"job_canonical",binding.job->canonical_json},{"job_fingerprint",binding.job->fingerprint},{"job_id",binding.job->job_id},
        {"job_revision",binding.job->input_revision},{"attempt",binding.attempt},
        {"initial_position",{candidate.initial_position.x(),candidate.initial_position.y(),candidate.initial_position.z()}},
        {"material_journal",ledger.fingerprint()},{"motion_policy",candidate.plan->policy_fingerprint},{"serializer_policy",candidate.policy_fingerprint},
        {"source_fingerprint",ledger.source_fingerprint},{"source_revision",ledger.revision}};
    if(binding.native){const auto &native=*binding.native;const auto &hatches=*native.hatches;const auto &body=*hatches.body;
        record["native"]={{"canonical",native.canonical_json},{"sha256",native.sha256},
            {"hatch_canonical",hatches.canonical_json},{"hatch_sha256",hatches.sha256},{"body_canonical",body.canonical_json},{"body_sha256",body.sha256}};
        const auto journal=[&](const char *name,const MaterialSequenceSnapshot &sequence){
            std::vector<std::string> rows;for(size_t i=0;i<sequence.records.size();++i)rows.push_back(sequence.canonical_record(i));
            record["native"][name]={{"context",sequence.canonical_context()},{"records",rows},{"sha256",sequence.fingerprint()}};
        };
        journal("assembled",*native.assembly->material->sequence);journal("planned",ledger);journal("body",*body.body->material);
    }
    REQUIRE_FALSE(boost::filesystem::exists(path));boost::nowide::ofstream file(path.string());REQUIRE(file.good());
    file<<record.dump(2)<<'\n';file.close();REQUIRE(file.good());
}
}
