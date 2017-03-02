#include <stdio.h>
#include <memory.h>
#include <assert.h>
#include <stdlib.h>

#include "../support/benchmark.h"
#include "process.h"

void usage(char *prg_name) {
    const char usage_string[] = " Run a bunch of small filters\n\n"
                                "\t -m -> hvx_mode - options are hvx64, hvx128. Default is to run hvx64, hvx128 and cpu\n"
                                "\t -n -> number of iterations\n"
                                "\t -h -> print this help message\n";
    printf ("%s - %s", prg_name, usage_string);

}

const char *to_string(bmark_run_mode_t mode) {
    if (mode == bmark_run_mode_t::hvx64) {
        return "(64 byte mode)";
    } else if (mode == bmark_run_mode_t::hvx128) {
        return "(128 byte mode)";
    } else {
        return "(cpu)";
    }
}

int main(int argc, char **argv) {
    // Set some defaults first.
    const int W = 1024;
    const int H = 1024;
    std::vector<bmark_run_mode_t> modes = {bmark_run_mode_t::hvx64, bmark_run_mode_t::hvx128, bmark_run_mode_t::cpu};
    int iterations = 10;

    // Process command line args.
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'm':
                {
                    std::string mode_to_run = argv[i+1];
                    modes.clear();
                    if (mode_to_run == "hvx64") {
                        modes.push_back(bmark_run_mode_t::hvx64);
                    } else if (mode_to_run == "hvx128") {
                        modes.push_back(bmark_run_mode_t::hvx128);
                    } else if (mode_to_run == "cpu") {
                        modes.push_back(bmark_run_mode_t::cpu);
                    } else {
                        usage(argv[0]);
                        abort();
                    }
                    i++;
                }
                break;
            case 'h':
                usage(argv[0]);
                return 0;
                break;
            case 'n':
                iterations = atoi(argv[i+1]);
                i++;
                break;
            }
        }
    }

    Conv3x3a16Descriptor conv3x3a16_pipeline(conv3x3a16_hvx64, conv3x3a16_hvx128, conv3x3a16_cpu, W, H);
    Dilate3x3Descriptor dilate3x3_pipeine(dilate3x3_hvx64, dilate3x3_hvx128, dilate3x3_cpu, W, H);
    Median3x3Descriptor median3x3_pipeline(median3x3_hvx64, median3x3_hvx128, median3x3_cpu, W, H);
    Gaussian5x5Descriptor gaussian5x5_pipeline(gaussian5x5_hvx64, gaussian5x5_hvx128, gaussian5x5_cpu, W, H);
    SobelDescriptor sobel_pipeline(sobel_hvx64, sobel_hvx128, sobel_cpu, W, H);
    Conv3x3a32Descriptor conv3x3a32_pipeline(conv3x3a32_hvx64, conv3x3a32_hvx128, conv3x3a32_cpu, W, H);


    std::vector<PipelineDescriptorBase *> pipelines = {&conv3x3a16_pipeline, &dilate3x3_pipeine, &median3x3_pipeline,
                                                       &gaussian5x5_pipeline, &sobel_pipeline, &conv3x3a32_pipeline};

    for (bmark_run_mode_t m : modes) {
        for (PipelineDescriptorBase *p : pipelines) {
            if (!p->defined()) {
                continue;
            }
            p->init();
            printf ("Running %s...\n", p->name());

            halide_hexagon_set_performance_mode(NULL, halide_hvx_power_turbo);
            halide_hexagon_power_hvx_on(NULL);

            double time = benchmark(iterations, 10, [&]() {
                    int result = p->run(m);
                    if (result != 0) {
                        printf("pipeline failed! %d\n", result);
                    }
                });
            printf("Done, time (%s): %g s %s\n", p->name(), time, to_string(m));

            // We're done with HVX, power it off.
            halide_hexagon_power_hvx_off(NULL);

            if (!p->verify(W, H)) {
                abort();
            }
        }
    }

    printf("Success!\n");
    return 0;
}
