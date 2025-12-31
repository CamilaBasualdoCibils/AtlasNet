#include "Misc/String_utils.hpp"
#include "MiscDockerFiles.hpp"

DOCKER_FILE_DEF CartographDockerFile =
	MacroParse(R"(
    
ARG CartographPath=${BOOTSTRAP_RUNTIME_SRC_DIR}/apps/Cartograph/
# ---------- Base image with Node ----------
FROM node:22-alpine AS base

# All paths inside the container will be relative to /app
WORKDIR /app

# ---------- Install dependencies ----------
FROM base AS deps
ARG CartographPath
# Copy only package files first (better Docker cache)
COPY ${CartographPath}web/package*.json ./web/

WORKDIR /app/web
RUN npm install

# ---------- Build the Next.js app ----------
FROM base AS builder

WORKDIR /app/web
ARG CartographPath
# Bring in node_modules from deps stage

# Copy the rest of your Next.js project from /web
COPY ${CartographPath}web/ ./
RUN rm -rf ./node_modules
RUN rm -rf ./.next

COPY --from=deps /app/web/node_modules ./node_modules
# Build for production
RUN npm run build

# ---------- Runtime image ----------
FROM node:22-alpine AS runner

WORKDIR /app/web
ENV NODE_ENV=production

# Copy built app + node_modules
COPY --from=builder /app/web ./

# Next.js default port
EXPOSE 3000

# Start your Next.js app
# (Assumes "start" script is defined in /web/package.json)
CMD ["node", "--inspect=0.0.0.0:9229", "node_modules/next/dist/bin/next", "dev"]


)",
			   {{"BOOTSTRAP_RUNTIME_SRC_DIR", BOOTSTRAP_RUNTIME_SRC_DIR}});