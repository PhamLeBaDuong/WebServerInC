# Use a base image with a C compiler
FROM gcc:latest

# Set the working directory
WORKDIR /app

# Copy the C source code to the container
COPY . .

RUN apt-get update && apt-get install -y make

# Compile the C code
RUN gcc -g server.c squeue.c -o server.exe

EXPOSE 9909

# Set the entrypoint to run the executable
CMD ["server.exe"]