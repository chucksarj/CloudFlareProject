# Arjun Loganathan (arlogana@ucsc.edu)

2021 Summer Internship Cloudflare worker + systems project.

Directories:
-
--->my_worker - general assesment portion
-------->index.js - javascript file with json api and html rewriter site
-------->URL.txt - text file with website url - https://my_worker.arlogana.workers.dev/
--->systems_proj - Systems portion of assignment
-------->CLI.c - my CLI tool used for sending requests to url's
-------->Makefile - simple Makefile with Macros to compile CLI


Notes:
-
* I thoroughly enjoyed doing this project as I learnt a lot about using wrangler and cloudflare (I had 0 prior experience) and I was able to brush up on my systems programming knowledge - thank you for the project opportunity!
* To compile CLI.c, run command 'make' on command line in systems_proj directory
* To run: ./CLI --url *required non HTTPS url* --profile *optional positive int*
* run with option --help or -h for more detailed help guide
* Supporting HTTPS would require a substantial effort that I did not think was neccessary.
* If I had more time, I wish I would've implemented more functionality for the user on the CLI tool side: things like having the response output in a passed file if requested, logging, supporting different kinds of requests (only supports GET requests as what kind of request was unspecified by spec), error handling gracefully, etc. 
* I wanted to submit ideally by the end of the weekend, but I ran into a lot of logic bugs. Actually the reason I used EPOLL was because I thought I had a seperate socket issue while debugging, but it turned out I had logic issues. I decided to keep EPOLL anyways as it it good for failure cases. 

