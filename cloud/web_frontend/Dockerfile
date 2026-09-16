FROM node:22-alpine AS builder

ARG VITE_API_BASE_URL=/api
ENV VITE_API_BASE_URL=$VITE_API_BASE_URL
ARG VITE_WS_URL=
ENV VITE_WS_URL=$VITE_WS_URL

WORKDIR /app

COPY package.json package-lock.json ./

RUN npm ci --registry=https://registry.npmmirror.com

COPY . .

RUN npm run build

FROM nginx:alpine

COPY --from=builder /app/dist /usr/share/nginx/html

EXPOSE 80