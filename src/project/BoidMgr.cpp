#include "BoidMgr.h"

BoidMgr::BoidMgr()
    : m_repulsionBorders(-Camera::GetOffset() * 5.0f, sf::Vector2f(Window::GetWidth() - 200.0f, Window::GetHeight()) * 5.0f),
      m_quadtreeRect(-Camera::GetOffset() * 6.0f, sf::Vector2f(Window::GetWidth() - 200.0f, Window::GetHeight()) * 6.0f),
      m_quadtreeBox(vl::Null<>(), sf::Vector2f(m_quadtreeRect.width, m_quadtreeRect.width) / 50.0f)
{
    SetBoidCount(200);
    for (auto &boid : m_boids)
    {
        sf::Vector2f randomForce;
        while ((randomForce = Random::Vec2(-1.0f, -1.0f, 1.0f, 1.0f) * 1000.0f) == vl::Null<>())
            ;
        boid.ApplyForce(randomForce);
    }

    // Setup quadtree
    m_quadtree.resize(m_quadtreeRect.width / m_quadtreeBox.width);
    for (auto &col : m_quadtree)
    {
        col.resize(m_quadtreeRect.height / m_quadtreeBox.height);
    }
}

void BoidMgr::Update()
{
    ClearQuadtree();
    ComputeQuadTree();
    CalculateAllVisibleNeighbors();
    for (auto &boid : m_boids)
    {
        boid.ApplyForce(boid.GetSeparationForce() * boid.GetSeparationMultiplier());
        boid.ApplyForce(boid.GetAlignmentForce() * boid.GetAlignmentMultiplier());
        boid.ApplyForce(boid.GetCohesionForce() * boid.GetCohesionMultiplier());
        boid.ApplyForce(GetRepulsionBorderForce(boid));
        boid.Update();
    }
    if (Keyboard::IsPressed(sf::Keyboard::Space))
        log_info("FPS: %f", Clock::GetFPS());
}

void BoidMgr::Draw()
{
    for (auto &boid : m_boids)
    {
        boid.DrawSight();
    }
    for (auto &boid : m_boids)
    {
        boid.DrawVisibleNeighbors();
    }
    for (auto &boid : m_boids)
    {
        boid.DrawVelocity();
    }
    for (auto &boid : m_boids)
    {
        boid.DrawBody();
    }

    Camera::DrawRect(m_repulsionBorders, sf::Color::Transparent, true, sf::Color::Red);
    DrawQuadTree();
}

void BoidMgr::DrawQuadTree()
{
    sf::RectangleShape rectShape(sf::Vector2f(m_quadtreeBox.width, m_quadtreeBox.height));
    rectShape.setFillColor(sf::Color::Transparent);
    rectShape.setOutlineThickness(1.0f / Camera::GetZoom());
    rectShape.setOutlineColor(sf::Color(255, 255, 255, 20));

    for (size_t i = 0; i < m_quadtree.size(); i++)
    {
        for (size_t j = 0; j < m_quadtree[i].size(); j++)
        {
            rectShape.setPosition(i * m_quadtreeBox.width + m_quadtreeRect.left,
                                  j * m_quadtreeBox.height + m_quadtreeRect.top);
            Camera::Draw(rectShape);
        }
    }

    rectShape.setOutlineColor(sf::Color::Red);
    for (auto &coord : m_activeContainers)
    {
        rectShape.setPosition(coord.first * m_quadtreeBox.width + m_quadtreeRect.left,
                              coord.second * m_quadtreeBox.height + m_quadtreeRect.top);
        Camera::Draw(rectShape);
    }
}

void BoidMgr::SetBoidCount(size_t count)
{
    while (m_boids.size() != count)
    {
        if (m_boids.size() < count)
        {
            sf::Vector2f randomPosition = Random::Vec2(-Camera::GetOffset(), sf::Vector2f(Window::GetWidth() - 200.0f, Window::GetHeight()));
            m_boids.push_back(Boid(randomPosition));
        }
        else
        {
            m_boids.pop_back();
        }
    }
}

void BoidMgr::SetSeparationMultiplier(float multiplier) noexcept
{
    for (auto &boid : m_boids)
    {
        boid.SetSeparationMultiplier(multiplier);
    }
}

void BoidMgr::SetAlignmentMultiplier(float multiplier) noexcept
{
    for (auto &boid : m_boids)
    {
        boid.SetAlignmentMultiplier(multiplier);
    }
}

void BoidMgr::SetCohesionMultiplier(float multiplier) noexcept
{
    for (auto &boid : m_boids)
    {
        boid.SetCohesionMultiplier(multiplier);
    }
}

void BoidMgr::CalculateAllVisibleNeighbors()
{
    for (auto &index : m_activeContainers)
    {
        for (auto &boid : m_quadtree[index.first][index.second])
        {
            boid->ClearNeighbors();
            boid->ClearVisibleNeighbors();

            float radius = boid->GetSightRadius();
            int nLayersToCheck = static_cast<int>(std::floor(static_cast<int>(static_cast<float>(std::min(m_quadtreeBox.width, m_quadtreeBox.height)) / radius) + 1));
            for (int i = 0; i < nLayersToCheck + 2; i++)
            {
                for (int j = 0; j < nLayersToCheck + 2; j++)
                {
                    for (auto &otherBoid : m_quadtree[index.first - 1 + i][index.second - 1 + j])
                    {
                        if (boid == otherBoid)
                            continue;

                        const sf::Vector2f _thisPos = boid->GetPosition();
                        const sf::Vector2f _otherPos = otherBoid->GetPosition();
                        const sf::Vector2f forward = boid->GetForward();
                        const auto sightBounds = boid->GetSightBounds();

                        const bool inSeeingDistance = vl::Distance(_thisPos, _otherPos) < boid->GetSightRadius();
                        const bool inPeripheral = !vl::IsLeft(_thisPos, sightBounds.first, _otherPos) && vl::IsLeft(_thisPos, sightBounds.second, _otherPos);
                        const bool inFront = vl::IsLeft(vl::Perpendicular(forward), _thisPos, _otherPos);
                        const bool angleBiggerThan180 = boid->GetSightAngle() > 180.0f;
                        const bool angleBiggerThan360 = boid->GetSightAngle() > 360.0f;

                        if (inSeeingDistance)
                        {
                            boid->AddNeighbor(otherBoid);
                        }

                        if (inSeeingDistance && (inPeripheral ||
                                                 (angleBiggerThan180 && inFront) ||
                                                 (angleBiggerThan360)))
                        {
                            boid->AddVisibleNeighbor(otherBoid);
                        }
                    }
                }
            }
        }
    }
}

sf::Vector2f BoidMgr::GetRepulsionBorderForce(const Boid &boid) const noexcept
{
    const sf::Vector2f &position = boid.GetPosition();
    auto &box = m_repulsionBorders;

    const auto xRange = std::make_pair(box.left, box.left + box.width);
    const auto yRange = std::make_pair(box.top, box.top + box.height);

    float fromLeft = Lib::Map(position.x, xRange, std::make_pair(-1.0f, 1.0f));
    float fromTop = Lib::Map(position.y, yRange, std::make_pair(-1.0f, 1.0f));
    float fromRight = -fromLeft;
    float fromBottom = -fromTop;

    fromLeft = Lib::Constrain(fromLeft, -100.0f, 1.0f);
    fromTop = Lib::Constrain(fromTop, -100.0f, 1.0f);
    fromRight = Lib::Constrain(fromRight, -100.0f, 1.0f);
    fromBottom = Lib::Constrain(fromBottom, -100.0f, 1.0f);

    sf::Vector2f force(0.0f, 0.0f);
    force.x += std::pow(fromLeft - 1.0f, 8.0f);
    force.x -= std::pow(fromRight - 1.0f, 8.0f);
    force.y += std::pow(fromTop - 1.0f, 8.0f);
    force.y -= std::pow(fromBottom - 1.0f, 8.0f);

    return force * 25.0f;
}

void BoidMgr::ComputeQuadTree()
{
    for (auto &boid : m_boids)
    {
        const sf::Vector2f &position = boid.GetPosition();
        sf::Vector2i middleIndex((position.x - m_quadtreeRect.left) / m_quadtreeBox.width, (position.y - m_quadtreeRect.top) / m_quadtreeBox.height);
        m_quadtree[middleIndex.x][middleIndex.y].push_back(&boid);
        m_activeContainers.emplace(middleIndex.x, middleIndex.y);
    }
}

void BoidMgr::ClearQuadtree() noexcept
{
    for (auto &col : m_quadtree)
    {
        for (auto &container : col)
        {
            container.clear();
        }
    }
    m_activeContainers.clear();
}
